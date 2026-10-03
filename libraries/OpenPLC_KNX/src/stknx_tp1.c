/*
 * stknx_tp1.c - see stknx_tp1.h.
 *
 * Runs entirely in two interrupts of the same priority (pulse capture and
 * bit-period tick), so the two never preempt each other. The main loop only
 * hands frames in and takes frames out, through tx_state and the rx queue.
 */

#include "stknx_tp1.h"
#include <string.h>

#ifndef STKNX_BARRIER
#  define STKNX_BARRIER() __sync_synchronize()
#endif

#define BIT       STKNX_BIT_US
#define HALF_BIT  (STKNX_BIT_US / 2u)

/* The stop bit is slot 10; once its middle has passed, no pulse can still
 * belong to this character. */
#define CHAR_CLOSE_US      (11u * BIT + HALF_BIT)
/* Inside a character the widest gap between pulses is 9 bit periods (start to
 * parity, all data bits 1), so 10 can only be a gap between characters. */
#define RESYNC_GAP_BITS    10u
/* Characters of one frame start 13 bit periods apart; anything outside this
 * window starts a new frame. */
#define FRAME_GAP_MIN_US   (11u * BIT)
#define FRAME_GAP_MAX_US   (17u * BIT)
#define FRAME_STALL_BITS   20u
#define CTRL_NOT_REPEATED  0x20u

enum { TX_NONE = 0, TX_PENDING, TX_SENDING, TX_WAIT_ACK, TX_DONE_OK, TX_DONE_FAIL };
enum { ACK_NONE = 0, ACK_DUE, ACK_SENDING };

/* --- Character and frame helpers ---------------------------------------- */

uint8_t stknx_even_parity(uint8_t b)
{
    b ^= (uint8_t)(b >> 4);
    b ^= (uint8_t)(b >> 2);
    b ^= (uint8_t)(b >> 1);
    return (uint8_t)(b & 1u);   /* 1 when the data holds an odd number of ones */
}

/* Logic 0 is an active pulse, logic 1 a silent bit period. */
uint16_t stknx_encode_char(uint8_t b)
{
    uint16_t m = 1u;                                   /* start bit */
    for (uint8_t i = 0u; i < 8u; i++) {
        if (((b >> i) & 1u) == 0u) {
            m |= (uint16_t)(1u << (1u + i));
        }
    }
    if (stknx_even_parity(b) == 0u) {
        m |= (uint16_t)(1u << 9);
    }
    return m;                                          /* stop bit: no pulse */
}

int stknx_decode_char(uint16_t pulses, uint8_t *out)
{
    uint8_t b = 0u;

    for (uint8_t i = 0u; i < 8u; i++) {
        if ((pulses & (1u << (1u + i))) == 0u) {
            b |= (uint8_t)(1u << i);
        }
    }
    *out = b;

    uint8_t parity = ((pulses & (1u << 9)) != 0u) ? 0u : 1u;
    return ((pulses & 1u) != 0u)
        && (parity == stknx_even_parity(b))
        && ((pulses & (1u << 10)) == 0u);
}

uint8_t stknx_checksum(const uint8_t *f, uint16_t n)
{
    uint8_t x = 0u;
    for (uint16_t i = 0u; i < n; i++) {
        x ^= f[i];
    }
    return (uint8_t)~x;
}

/* Layouts as in the stack's TpFrame (knx/tp_frame.h). */
static uint8_t is_extended(const uint8_t *f)
{
    return (uint8_t)((f[0] & 0xD3u) == 0x10u);
}

uint16_t stknx_frame_length(const uint8_t *f, uint16_t n)
{
    if (n == 0u) {
        return 0u;
    }
    if (is_extended(f)) {
        return (n >= 7u) ? (uint16_t)(9u + f[6]) : 0u;
    }
    return (n >= 6u) ? (uint16_t)(8u + (f[5] & 0x0Fu)) : 0u;
}

/* Masks as in $BOOT/TestCase/KNX/knx_test.c KNX_Test_AckKind(). A control
 * field always has bit 4 set, so a frame's first octet never reads as one. */
uint8_t stknx_ack_kind(uint8_t octet)
{
    if ((octet & 0x33u) != 0u) {
        return STKNX_ACKKIND_NONE;
    }
    if (((octet & 0x0Cu) != 0u) && ((octet & 0xC0u) != 0u)) {
        return STKNX_ACKKIND_ACK;
    }
    if ((octet & 0xC0u) == 0u) {
        return STKNX_ACKKIND_NAK;
    }
    return STKNX_ACKKIND_BUSY;
}

/* --- Transmit outcome ----------------------------------------------------- */

static void tx_finish(stknx_link_t *l, uint8_t ok)
{
    if (ok) { l->tx_ok++; } else { l->tx_failed++; }
    l->tx_state = ok ? TX_DONE_OK : TX_DONE_FAIL;
}

static void tx_repeat(stknx_link_t *l, uint8_t busy)
{
    if (l->tx_repeats >= STKNX_REPEATS) {
        tx_finish(l, 0u);
        return;
    }
    l->tx_repeats++;
    l->tx[0] &= (uint8_t)~CTRL_NOT_REPEATED;
    l->tx[l->tx_len - 1u] = stknx_checksum(l->tx, (uint16_t)(l->tx_len - 1u));
    l->tx_need_idle = busy ? STKNX_IDLE_AFTER_BUSY_BITS : STKNX_IDLE_BEFORE_TX_BITS;
    l->tx_state = TX_PENDING;
}

static void ack_received(stknx_link_t *l, uint8_t kind)
{
    if (l->tx_state != TX_WAIT_ACK) {
        return;   /* our own acknowledge, or one meant for somebody else */
    }
    if (kind == STKNX_ACKKIND_ACK) {
        tx_finish(l, 1u);
    } else {
        tx_repeat(l, (uint8_t)(kind == STKNX_ACKKIND_BUSY));
    }
}

/* --- Receive: frames ----------------------------------------------------- */

static void rf_check_addressed(stknx_link_t *l)
{
    const uint8_t *f = l->rf;
    uint16_t dst;
    uint8_t  grp;

    if (l->rf_echo || (l->addressed == 0)) {
        l->rf_addressed = 0u;
        return;
    }
    if (is_extended(f)) {
        dst = (uint16_t)(((uint16_t)f[4] << 8) | f[5]);
        grp = (uint8_t)(f[1] >> 7);
    } else {
        dst = (uint16_t)(((uint16_t)f[3] << 8) | f[4]);
        grp = (uint8_t)(f[5] >> 7);
    }
    l->rf_addressed = (uint8_t)(l->addressed(l->ctx, dst, grp) != 0u);
}

static void frame_done(stknx_link_t *l, uint16_t t0_last)
{
    uint16_t n = l->rf_len;
    uint8_t  reply;

    l->rf_len = 0u;
    if (l->rf_echo) {
        return;   /* our own frame coming back */
    }

    uint8_t good = (uint8_t)(stknx_checksum(l->rf, (uint16_t)(n - 1u)) == l->rf[n - 1u]);
    if (!good) {
        l->rx_bad++;
    }
    if (!l->rf_addressed) {
        return;
    }

    if (!good) {
        reply = STKNX_NAK;
    } else {
        uint8_t h    = l->rxq_head;
        uint8_t next = (uint8_t)((h + 1u) % STKNX_RX_QUEUE);
        if (next == l->rxq_tail) {
            reply = STKNX_BUSY;
            l->rx_busy++;
        } else {
            memcpy(l->rxq[h], l->rf, n);
            l->rxq_len[h] = n;
            STKNX_BARRIER();
            l->rxq_head = next;
            l->rx_frames++;
            reply = STKNX_ACK;
        }
    }

    /* The acknowledge starts 15 bit periods after the last character's stop bit. */
    l->ack_state = ACK_DUE;
    l->ack_octet = reply;
    l->ack_at    = (uint16_t)(t0_last + (11u + STKNX_ACK_GAP_BITS) * BIT);
}

static void frame_char(stknx_link_t *l, uint8_t b, int ok, uint16_t t0)
{
    if (!ok) {
        if (l->rf_len != 0u) { l->rx_bad++; }
        l->rf_len    = 0u;
        l->ch_resync = 1u;
        return;
    }
    if (l->rf_len != 0u) {
        uint16_t gap = (uint16_t)(t0 - l->rf_prev_t0);
        if ((gap < FRAME_GAP_MIN_US) || (gap > FRAME_GAP_MAX_US)) {
            l->rx_bad++;
            l->rf_len = 0u;
        }
    }
    if (l->rf_len == 0u) {
        uint8_t kind = stknx_ack_kind(b);
        if (kind != STKNX_ACKKIND_NONE) {
            ack_received(l, kind);
            return;
        }
        l->rf_echo      = (uint8_t)(l->tx_state == TX_SENDING);
        l->rf_addressed = 0u;
    }

    l->rf[l->rf_len++] = b;
    l->rf_prev_t0 = t0;
    if (l->rf_len == 6u) {
        rf_check_addressed(l);
    }
    uint16_t total = stknx_frame_length(l->rf, l->rf_len);
    if ((total != 0u) && (l->rf_len >= total)) {
        frame_done(l, t0);
    }
}

/* --- Receive: characters ------------------------------------------------- */

static void char_close(stknx_link_t *l)
{
    uint8_t b;
    int ok = stknx_decode_char(l->ch_pulses, &b);

    l->ch_active = 0u;
    frame_char(l, b, ok, l->ch_t0);
}

static void char_pulse(stknx_link_t *l, uint16_t t, uint16_t idle_bits)
{
    if (l->ch_active) {
        uint16_t idx = (uint16_t)(((uint16_t)(t - l->ch_t0) + HALF_BIT) / BIT);
        if (idx <= 10u) {
            l->ch_pulses |= (uint16_t)(1u << idx);
            l->last_pulse = t;
            return;
        }
        char_close(l);
    }

    /* This pulse is a start bit - unless a bad character left the decoder
     * unsure where characters begin. */
    if (l->ch_resync && l->have_pulse && (idle_bits < RESYNC_GAP_BITS)
        && ((uint16_t)(t - l->last_pulse) < (RESYNC_GAP_BITS * BIT))) {
        l->last_pulse = t;
        return;
    }
    l->ch_resync  = 0u;
    l->ch_active  = 1u;
    l->ch_t0      = t;
    l->ch_pulses  = 1u;
    l->last_pulse = t;
    l->have_pulse = 1u;
}

/* --- Engine ----------------------------------------------------------------- */

void stknx_link_init(stknx_link_t *l, stknx_addressed_fn addressed, void *ctx)
{
    memset(l, 0, sizeof(*l));
    l->addressed  = addressed;
    l->ctx        = ctx;
    l->p_len      = BIT;
    l->p_len_next = BIT;
}

uint8_t stknx_link_pulse(stknx_link_t *l, uint16_t t_us)
{
    uint8_t  silence = 0u;
    uint16_t idle    = l->idle_bits;

    /* Which bit period it started in: the one the last tick opened, or the
     * next one when this interrupt ran before that tick. */
    int16_t d       = (int16_t)(uint16_t)(t_us - l->p_start);
    uint8_t in_next = (uint8_t)(d >= (int16_t)(l->p_len / 2u));
    uint8_t emitted = in_next ? l->emit_next : l->emit_cur;
    uint8_t ours    = in_next ? l->tx_next   : l->tx_cur;

    if ((l->tx_state == TX_SENDING) && ours && !emitted) {
        /* Somebody else's 0 against our 1: arbitration lost. Stop at once;
         * the frame on the bus is theirs and is received normally. */
        l->collisions++;
        l->emit_next = 0u;
        l->tx_next   = 0u;
        l->tx_cur    = 0u;
        l->rf_echo   = 0u;
        if (l->rf_len >= 6u) {
            rf_check_addressed(l);
        }
        if (++l->tx_losses > STKNX_ARB_LOSSES_MAX) {
            tx_finish(l, 0u);
        } else {
            l->tx_need_idle = STKNX_IDLE_BEFORE_TX_BITS;
            l->tx_state     = TX_PENDING;
        }
        silence = 1u;
    }

    l->idle_bits = 0u;
    char_pulse(l, t_us, idle);
    return silence;
}

uint8_t stknx_link_tick(stknx_link_t *l, uint16_t now_us, uint16_t *next_len_us)
{
    if (l->idle_bits < 0xFFFFu) {
        l->idle_bits++;
    }

    /* The period that starts now is the one programmed at the last tick. */
    l->p_start  = now_us;
    l->p_len    = l->p_len_next;
    l->emit_cur = l->emit_next;
    l->tx_cur   = l->tx_next;

    uint16_t start_next = (uint16_t)(now_us + l->p_len);
    l->p_len_next = BIT;
    l->emit_next  = 0u;
    l->tx_next    = 0u;

    if (l->ch_active && ((uint16_t)(now_us - l->ch_t0) >= CHAR_CLOSE_US)) {
        char_close(l);
        /* Idle is counted from the stop bit, not from the last pulse: a
         * character ending in 1s has its last pulse up to 10 bits earlier. */
        l->idle_bits = 0u;
    }
    if ((l->rf_len != 0u) && (l->idle_bits > FRAME_STALL_BITS)) {
        l->rx_bad++;
        l->rf_len = 0u;
    }

    /* An owed acknowledge goes first and must start on time. */
    if (l->ack_state == ACK_SENDING) {
        if (l->ack_slot < 11u) {
            l->emit_next = (uint8_t)((l->ack_bits >> l->ack_slot) & 1u);
            l->ack_slot++;
        } else {
            l->ack_state = ACK_NONE;
        }
    } else if (l->ack_state == ACK_DUE) {
        int16_t diff = (int16_t)(uint16_t)(l->ack_at - start_next);
        if (diff < (int16_t)HALF_BIT) {
            l->ack_bits  = stknx_encode_char(l->ack_octet);
            l->emit_next = (uint8_t)(l->ack_bits & 1u);
            l->ack_slot  = 1u;
            l->ack_state = ACK_SENDING;
        } else if (diff < (int16_t)(BIT + HALF_BIT)) {
            /* Stretch or shrink the next period so the one after it starts
             * exactly on the acknowledge. */
            l->p_len_next = (uint16_t)diff;
        }
    } else {
        switch (l->tx_state) {
        case TX_PENDING:
            if (l->ch_active || (l->rf_len != 0u) || (l->idle_bits < l->tx_need_idle)) {
                break;
            }
            l->tx_state = TX_SENDING;
            l->tx_char  = 0u;
            l->tx_slot  = 0u;
            l->tx_bits  = stknx_encode_char(l->tx[0]);
            /* fall through */
        case TX_SENDING:
            if (l->tx_char >= l->tx_len) {
                l->tx_state = TX_WAIT_ACK;
                l->tx_wait  = 0u;
                break;
            }
            l->emit_next = (uint8_t)((l->tx_bits >> l->tx_slot) & 1u);
            l->tx_next   = 1u;
            if (++l->tx_slot >= STKNX_CHAR_SLOTS) {
                l->tx_slot = 0u;
                if (++l->tx_char < l->tx_len) {
                    l->tx_bits = stknx_encode_char(l->tx[l->tx_char]);
                }
            }
            break;
        case TX_WAIT_ACK:
            if (!l->ch_active && (++l->tx_wait > STKNX_ACK_WAIT_BITS)) {
                tx_repeat(l, 0u);
            }
            break;
        default:
            break;
        }
    }

    *next_len_us = l->p_len_next;
    return l->emit_next;
}

/* --- Main loop side -------------------------------------------------------- */

int stknx_link_send(stknx_link_t *l, const uint8_t *f, uint16_t n)
{
    if ((n < 2u) || (n > STKNX_FRAME_MAX) || (l->tx_state != TX_NONE)) {
        return 0;
    }
    memcpy(l->tx, f, n);
    l->tx_len       = n;
    l->tx_repeats   = 0u;
    l->tx_losses    = 0u;
    l->tx_need_idle = STKNX_IDLE_BEFORE_TX_BITS;
    STKNX_BARRIER();
    l->tx_state = TX_PENDING;
    return 1;
}

int stknx_link_send_result(stknx_link_t *l)
{
    uint8_t s = l->tx_state;

    if ((s != TX_DONE_OK) && (s != TX_DONE_FAIL)) {
        return STKNX_SEND_PENDING;
    }
    STKNX_BARRIER();
    l->tx_state = TX_NONE;
    return (s == TX_DONE_OK) ? STKNX_SEND_OK : STKNX_SEND_FAILED;
}

uint16_t stknx_link_receive(stknx_link_t *l, uint8_t *out, uint16_t cap)
{
    uint8_t t = l->rxq_tail;

    if (t == l->rxq_head) {
        return 0u;
    }
    STKNX_BARRIER();
    uint16_t n = l->rxq_len[t];
    if (n > cap) {
        n = 0u;   /* caller's buffer is too small: drop rather than truncate */
    } else {
        memcpy(out, l->rxq[t], n);
    }
    STKNX_BARRIER();
    l->rxq_tail = (uint8_t)((t + 1u) % STKNX_RX_QUEUE);
    return n;
}
