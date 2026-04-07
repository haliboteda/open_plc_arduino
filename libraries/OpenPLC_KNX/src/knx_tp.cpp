#include "knx_tp.h"
#include "knx_config.h"
#include <Arduino.h>
#include <string.h>

/* Dedicated HardwareSerial instance for the STKNX transceiver (USART1).
 *
 * We construct it from the peripheral handle (USART1) rather than relying on
 * the globally pre-instantiated Serial1, because HAVE_HWSERIAL1 is not defined
 * in the OpenPLC variant and Serial1 would produce an undefined-reference linker
 * error.  The HardwareSerial(void *peripheral) constructor is provided by the
 * STM32duino core for exactly this use case.
 *
 * USART1 pin assignment (from Bridge MPU schematic):
 *   TX = PB14  (USART1_TX, AF4)   — connected to STKNX via TLP2362 optocoupler
 *   RX = PA10  (USART1_RX, AF7)   — connected from STKNX via TLP2362 optocoupler
 */
static HardwareSerial s_knx_serial((void *)USART1);

/* All UART access in this file goes through this macro */
#define KNX_TP_SERIAL  s_knx_serial

/* -------------------------------------------------------------------------
 * RX state machine states
 * ---------------------------------------------------------------------- */
typedef enum {
    RX_IDLE,        /* Waiting for a frame indicator byte or a control byte */
    RX_FRAME,       /* Accumulating KNX frame bytes */
    RX_INVALID      /* Frame in progress is corrupt — discard until next gap */
} RxState;

/* -------------------------------------------------------------------------
 * Internal state
 * ---------------------------------------------------------------------- */

static knx_tp_rx_cb_t    s_rx_cb       = NULL;
static bool              s_connected   = false;
static KnxIndividualAddr s_own_addr    = 0u;

/* RX state machine */
static RxState  s_rx_state  = RX_IDLE;
static uint8_t  s_rxbuf[KNX_TP_RX_BUF_SIZE];
static uint8_t  s_rxbuf_len = 0u;
static uint32_t s_rx_last_ms = 0u;   /* millis() of last received byte */

/* TX confirmation tracking */
static bool     s_tx_pending    = false;   /* true while waiting for L_DATA_CON */
static uint32_t s_tx_sent_ms    = 0u;      /* millis() when frame was sent */
#define TX_CON_TIMEOUT_MS       500u       /* max wait for L_DATA_CON */

/* Debounce state for programming key */
static bool     s_prog_key_state = false;
static uint32_t s_prog_key_time  = 0u;

/* -------------------------------------------------------------------------
 * Internal helpers
 * ---------------------------------------------------------------------- */

/* Expected total frame length in bytes, determined from the DAF byte (index 5).
 * Total = 6-byte header + APDU (bits3-0 + 1 bytes) + 1-byte checksum. */
static inline uint8_t frame_expected_len(const uint8_t *buf)
{
    return (uint8_t)(6u + (buf[5] & 0x0Fu) + 1u + 1u);
}

/* Classify an incoming byte as either a frame indicator, a control byte,
 * or an unknown byte.  Returns true if the byte looks like a KNX frame
 * start (standard or extended). */
static inline bool is_frame_indicator(uint8_t b)
{
    return ((b & TPUART_DATA_STD_MASK) == TPUART_DATA_STD_IND)
        || ((b & TPUART_DATA_STD_MASK) == TPUART_DATA_EXT_IND);
}

/* Deliver the current RX buffer contents as a KnxTelegram if valid,
 * then reset the buffer. */
static void deliver_and_reset(void)
{
    if (s_rxbuf_len >= KNX_FRAME_MIN_LEN) {
        KnxTelegram tg;
        if (knxTelegram_deserialize(&tg, s_rxbuf, s_rxbuf_len)) {
            if (s_rx_cb != NULL) {
                s_rx_cb(&tg);
            }
        }
    }
    s_rxbuf_len = 0u;
    s_rx_state  = RX_IDLE;
}

/* -------------------------------------------------------------------------
 * Initialization
 *
 * Performs the TP-UART reset handshake:
 *   1. Send U_RESET_REQ (0x01)
 *   2. Wait up to 10 ms for U_RESET_IND (0x03)
 *   3. Program own individual address into chip (U_SET_ADDRESS_REQ 0xF1)
 *   4. Send U_STATE_REQ (0x02) to read chip status
 * ---------------------------------------------------------------------- */

bool knx_tp_init(KnxIndividualAddr own_addr)
{
    s_own_addr   = own_addr;
    s_connected  = false;
    s_rx_state   = RX_IDLE;
    s_rxbuf_len  = 0u;
    s_tx_pending = false;

    /* Configure GPIO pins */
    pinMode(KNX_TP_OK_PIN,       INPUT);
    pinMode(KNX_TP_VCC_OK_PIN,   INPUT);
    pinMode(KNX_TP_PROG_KEY_PIN, INPUT_PULLUP);
    pinMode(KNX_TP_PROG_LED_PIN, OUTPUT);
    digitalWrite(KNX_TP_PROG_LED_PIN, LOW);

    /* Configure UART */
    KNX_TP_SERIAL.begin(KNX_TP_BAUD, KNX_TP_SERIAL_CONFIG);
    delay(5);  /* short settle time after UART init */

    /* Flush any stale bytes */
    while (KNX_TP_SERIAL.available()) {
        (void)KNX_TP_SERIAL.read();
    }

    /* Step 1: Send reset request */
    KNX_TP_SERIAL.write(TPUART_RESET_REQ);

    /* Step 2: Wait for U_RESET_IND (0x03) within 10 ms */
    const uint32_t t0 = millis();
    bool got_reset_ind = false;
    while ((millis() - t0) < 10u) {
        if (KNX_TP_SERIAL.available()) {
            uint8_t b = (uint8_t)KNX_TP_SERIAL.read();
            if (b == TPUART_RESET_IND) {
                got_reset_ind = true;
                break;
            }
        }
    }

    if (!got_reset_ind) {
        /* Chip did not respond — bus may be unpowered or chip in error state.
         * Leave s_connected = false so callers can detect this. */
        return false;
    }

    /* Step 3: Program own individual address for auto-ACK of unicast frames */
    KNX_TP_SERIAL.write(TPUART_SET_ADDR_REQ);
    KNX_TP_SERIAL.write((uint8_t)(own_addr >> 8));
    KNX_TP_SERIAL.write((uint8_t)(own_addr & 0xFFu));

    /* Step 4: Request chip status */
    KNX_TP_SERIAL.write(TPUART_STATE_REQ);

    s_connected  = true;
    s_rx_last_ms = millis();
    return true;
}

/* -------------------------------------------------------------------------
 * Callback registration
 * ---------------------------------------------------------------------- */

void knx_tp_set_rx_callback(knx_tp_rx_cb_t cb)
{
    s_rx_cb = cb;
}

/* -------------------------------------------------------------------------
 * Transmission — TP-UART slot-byte protocol
 *
 * For each byte at position i in the serialized KNX frame:
 *   - Non-last byte: send (0x80 | i), then the byte
 *   - Last byte:     send (0x40 | i), then the byte
 *
 * After sending, we set s_tx_pending = true and wait for L_DATA_CON
 * in knx_tp_poll().  A timeout of TX_CON_TIMEOUT_MS clears the flag
 * if confirmation never arrives (e.g. bus collision).
 * ---------------------------------------------------------------------- */

bool knx_tp_send(const KnxTelegram *tg)
{
    if (!s_connected) {
        return false;
    }

    uint8_t frame[KNX_FRAME_MAX_LEN];
    uint8_t len = knxTelegram_serialize(tg, frame, (uint8_t)sizeof(frame));
    if (len == 0u) {
        return false;
    }

    for (uint8_t i = 0u; i < len; i++) {
        if (i == (len - 1u)) {
            KNX_TP_SERIAL.write((uint8_t)(TPUART_TX_END   | i)); /* 0x40 | pos */
        } else {
            KNX_TP_SERIAL.write((uint8_t)(TPUART_TX_START | i)); /* 0x80 | pos */
        }
        KNX_TP_SERIAL.write(frame[i]);
    }

    s_tx_pending = true;
    s_tx_sent_ms = millis();
    return true;
}

/* -------------------------------------------------------------------------
 * Receive poll — byte-by-byte TP-UART state machine
 *
 * States:
 *   RX_IDLE    — waiting for a frame indicator or control byte
 *   RX_FRAME   — accumulating KNX frame bytes
 *   RX_INVALID — corrupt frame in progress, discard until next idle gap
 *
 * ACK behaviour (from TP-UART specification):
 *   When a new frame indicator arrives: immediately send U_ACK_REQ (0x10)
 *   as a no-commit placeholder (required by spec so the chip knows a host
 *   is present).
 *   After 7 bytes (destination address fully received): if the frame is
 *   addressed to own_addr or has a group destination (we ACK all group
 *   frames since our group object table may contain it), send positive ACK
 *   U_ACK_REQ | U_ACK_ADDRESSED (0x11).
 *
 * L_DATA_CON handling:
 *   The chip sends a confirmation byte after TX.  bit7=1 means success.
 *   We clear s_tx_pending regardless of the outcome.
 *
 * U_RESET_IND:
 *   If the chip resets unexpectedly, re-run the init sequence.
 * ---------------------------------------------------------------------- */

void knx_tp_poll(void)
{
    /* ---- TX confirmation timeout ---- */
    if (s_tx_pending && (millis() - s_tx_sent_ms) >= TX_CON_TIMEOUT_MS) {
        /* L_DATA_CON not received in time — clear pending flag */
        s_tx_pending = false;
    }

    /* ---- RX_INVALID recovery: clear after a 3 ms gap with no data ---- */
    if (s_rx_state == RX_INVALID
        && s_rxbuf_len == 0u
        && (millis() - s_rx_last_ms) > 3u
        && KNX_TP_SERIAL.available() == 0) {
        s_rx_state = RX_IDLE;
    }

    /* ---- Process all available UART bytes ---- */
    while (KNX_TP_SERIAL.available() > 0) {
        uint8_t b = (uint8_t)KNX_TP_SERIAL.read();
        s_rx_last_ms = millis();

        if (s_rx_state == RX_FRAME) {
            /* ---- Accumulate frame bytes ---- */
            if (s_rxbuf_len < KNX_TP_RX_BUF_SIZE) {
                s_rxbuf[s_rxbuf_len++] = b;
            } else {
                /* Buffer overflow — frame too long, discard */
                s_rxbuf_len = 0u;
                s_rx_state  = RX_INVALID;
                continue;
            }

            /* After 7 bytes the destination address is complete (bytes 3-4)
             * and the DAF byte (5) tells us the frame length.
             * Send ACK to chip now. */
            if (s_rxbuf_len == 7u) {
                bool is_group  = (s_rxbuf[5] & 0x80u) != 0u;
                uint16_t dst   = (uint16_t)(((uint16_t)s_rxbuf[3] << 8) | s_rxbuf[4]);
                bool for_me    = is_group || (dst == s_own_addr);

                if (!s_tx_pending) {
                    if (for_me) {
                        /* Positive ACK: we are addressed */
                        KNX_TP_SERIAL.write((uint8_t)(TPUART_ACK_REQ | TPUART_ACK_ADDRESSED));
                    }
                    /* If not for us, no further ACK is needed (0x10 was already sent) */
                }
            }

            /* Check if the frame is complete */
            if (s_rxbuf_len >= 6u) {
                uint8_t expected = frame_expected_len(s_rxbuf);
                if (s_rxbuf_len == expected) {
                    deliver_and_reset();
                }
            }

        } else if (s_rx_state == RX_IDLE) {
            /* ---- Look for a frame indicator or handle control bytes ---- */

            if (is_frame_indicator(b)) {
                /* This byte is simultaneously the frame indicator AND the
                 * KNX ctrl1 byte.  Store it as the first frame byte. */
                s_rxbuf_len  = 0u;
                s_rxbuf[s_rxbuf_len++] = b;
                s_rx_state   = RX_FRAME;

                /* Send no-commit placeholder ACK so the chip knows a host
                 * is present.  Not sent while we are transmitting. */
                if (!s_tx_pending) {
                    KNX_TP_SERIAL.write(TPUART_ACK_REQ);
                }

            } else if (b == TPUART_RESET_IND) {
                /* Chip spontaneously reset — reconnect */
                s_connected = false;
                knx_tp_init(s_own_addr);
                return;

            } else if ((b & TPUART_STATE_MASK) == TPUART_STATE_IND) {
                /* Status / error flags byte — ignore for now (no error LED) */

            } else if ((b & TPUART_CON_MASK) == TPUART_CON_IND) {
                /* L_DATA_CON: TX confirmation */
                s_tx_pending = false;
                /* bit7 of b: 1 = success, 0 = failure (collision / no ACK) */

            } else {
                /* Unknown control byte — ignore */
            }

        } else {
            /* RX_INVALID: discard byte */
        }
    }

    /* ---- Debounce programming key ---- */
    bool raw_key = (digitalRead(KNX_TP_PROG_KEY_PIN) == LOW);
    if (raw_key != s_prog_key_state) {
        if ((millis() - s_prog_key_time) >= KNX_PROG_DEBOUNCE_MS) {
            s_prog_key_state = raw_key;
            s_prog_key_time  = millis();
        }
    } else {
        s_prog_key_time = millis();
    }
}

/* -------------------------------------------------------------------------
 * Status accessors
 * ---------------------------------------------------------------------- */

bool knx_tp_connected(void)
{
    return s_connected;
}

bool knx_tp_bus_ok(void)
{
    return digitalRead(KNX_TP_OK_PIN) == HIGH;
}

bool knx_tp_vcc_ok(void)
{
    return digitalRead(KNX_TP_VCC_OK_PIN) == HIGH;
}

void knx_tp_prog_led_set(bool on)
{
    digitalWrite(KNX_TP_PROG_LED_PIN, on ? HIGH : LOW);
}

bool knx_tp_prog_key_pressed(void)
{
    return s_prog_key_state;
}
