#pragma once

/*
 * stknx_tp1.h - KNX TP1 data link engine for a bare STKNX transceiver.
 *
 * Pure logic, no HAL: the timer driver (stknx_phy.cpp) feeds it pulse
 * timestamps and bit-period ticks and applies what it returns. That keeps
 * every rule here testable on the host (tests/knx_tp1).
 *
 * Design and the TP1 rules it implements: $PROD/docs/modules/M3/KNX-TP-DATA-LINK.md
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Bit timing, STKNX datasheet DocID031327 Rev 1 section 5.1. */
#define STKNX_BIT_US                104u
#define STKNX_PULSE_US               35u
/* start, 8 data, parity, stop, then 2 idle bits before the next character */
#define STKNX_CHAR_SLOTS             13u

#define STKNX_FRAME_MAX             264u   /* extended frame: 9 header + 255 */
#define STKNX_RX_QUEUE                3u

#define STKNX_ACK                  0xCCu
#define STKNX_NAK                  0x0Cu
#define STKNX_BUSY                 0xC0u

#define STKNX_IDLE_BEFORE_TX_BITS    50u
#define STKNX_IDLE_AFTER_BUSY_BITS  150u
#define STKNX_ACK_GAP_BITS           15u   /* end of a frame to its acknowledge */
#define STKNX_ACK_WAIT_BITS          40u   /* after our frame, give up on an acknowledge */
#define STKNX_REPEATS                 3u
#define STKNX_ARB_LOSSES_MAX         20u

enum {
    STKNX_ACKKIND_NONE = 0,   /* not an acknowledge octet */
    STKNX_ACKKIND_ACK,
    STKNX_ACKKIND_NAK,
    STKNX_ACKKIND_BUSY
};

enum {
    STKNX_SEND_PENDING = 0,   /* nothing finished since the last call */
    STKNX_SEND_OK,
    STKNX_SEND_FAILED
};

/* --- Character and frame helpers ---------------------------------------- */

uint8_t  stknx_even_parity(uint8_t b);
/* Bit i set = an active pulse in slot i (0 start, 1..8 data LSB first, 9 parity, 10 stop). */
uint16_t stknx_encode_char(uint8_t b);
/* 1 when start, parity and stop are all right; *out gets the data either way. */
int      stknx_decode_char(uint16_t pulses, uint8_t *out);
/* The check octet over n octets: their XOR, inverted. */
uint8_t  stknx_checksum(const uint8_t *f, uint16_t n);
/* Total octets of the frame starting at f, check octet included; 0 until enough has arrived. */
uint16_t stknx_frame_length(const uint8_t *f, uint16_t n);
uint8_t  stknx_ack_kind(uint8_t octet);

/* --- The engine ----------------------------------------------------------- */

/* Asked once per received frame, from interrupt context, when its destination
 * is known: nonzero = this device takes the frame and acknowledges it. */
typedef uint8_t (*stknx_addressed_fn)(void *ctx, uint16_t dst, uint8_t is_group);

typedef struct {
    stknx_addressed_fn addressed;
    void              *ctx;

    /* bit-period bookkeeping */
    uint16_t p_start;          /* start of the period the last tick opened */
    uint16_t p_len;            /* its length, us */
    uint16_t p_len_next;       /* length programmed for the period after it */
    uint8_t  emit_cur;         /* that period carries an active pulse */
    uint8_t  emit_next;        /* the period after it does */
    uint8_t  tx_cur;           /* that period belongs to a frame we transmit */
    uint8_t  tx_next;
    uint16_t idle_bits;        /* whole bit periods since the last pulse */

    /* character decoder */
    uint8_t  ch_active;
    uint8_t  ch_resync;        /* after a bad character: wait for a long gap */
    uint16_t ch_t0;
    uint16_t ch_pulses;
    uint16_t last_pulse;
    uint8_t  have_pulse;

    /* frame being received */
    uint8_t  rf[STKNX_FRAME_MAX];
    uint16_t rf_len;
    uint16_t rf_prev_t0;
    uint8_t  rf_echo;
    uint8_t  rf_addressed;

    /* acknowledge we owe */
    uint8_t  ack_state;        /* 0 none, 1 due, 2 sending */
    uint8_t  ack_octet;
    uint16_t ack_at;
    uint16_t ack_bits;
    uint8_t  ack_slot;

    /* frame we transmit */
    volatile uint8_t tx_state;
    uint8_t  tx[STKNX_FRAME_MAX];
    uint16_t tx_len;
    uint16_t tx_char;
    uint8_t  tx_slot;
    uint16_t tx_bits;
    uint16_t tx_need_idle;
    uint8_t  tx_repeats;
    uint8_t  tx_losses;
    uint16_t tx_wait;

    /* received frames handed to the main loop */
    uint8_t  rxq[STKNX_RX_QUEUE][STKNX_FRAME_MAX];
    uint16_t rxq_len[STKNX_RX_QUEUE];
    volatile uint8_t rxq_head;
    volatile uint8_t rxq_tail;

    /* counters */
    uint32_t rx_frames, rx_bad, rx_busy, tx_ok, tx_failed, collisions;
} stknx_link_t;

void stknx_link_init(stknx_link_t *l, stknx_addressed_fn addressed, void *ctx);

/* Capture interrupt: the start of an active pulse on the bus, 1 us per count.
 * Returns 1 when the period already programmed must be made silent at once -
 * this device lost arbitration. */
uint8_t stknx_link_pulse(stknx_link_t *l, uint16_t t_us);

/* Bit-engine interrupt, at the start of every bit period. Returns whether the
 * period after this one carries an active pulse, and its length. */
uint8_t stknx_link_tick(stknx_link_t *l, uint16_t now_us, uint16_t *next_len_us);

/* Main loop. send: 1 = taken, 0 = a frame is still in flight or n is out of range. */
int      stknx_link_send(stknx_link_t *l, const uint8_t *f, uint16_t n);
int      stknx_link_send_result(stknx_link_t *l);
/* One received frame (check octet included) into out, or 0 when none is waiting. */
uint16_t stknx_link_receive(stknx_link_t *l, uint8_t *out, uint16_t cap);

#ifdef __cplusplus
}
#endif
