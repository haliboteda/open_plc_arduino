#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "knx_telegram.h"
#include "knx_address.h"

/*
 * KNX TP transport layer — STKNX (STMicroelectronics) via USART1.
 *
 * Hardware path:
 *   STM32H743 USART1  <->  TLP2362 optocoupler  <->  STKNX (U9)  <->  KNX TP bus
 *
 * STKNX uses the standard TP-UART protocol (same command byte set as NCN5120 base mode).
 * UART parameters: 19200 bps, 8 data bits, even parity, 1 stop bit (SERIAL_8E1).
 * Source: STMicroelectronics STKNX datasheet; confirmed by thelsing/knx reference.
 *
 * Protocol summary:
 *
 *   Initialization:
 *     Host  ->  0x01 (U_RESET_REQ)
 *     STKNX ->  0x03 (U_RESET_IND)  within 10 ms
 *     Host  ->  0xF1 addrH addrL  (U_SET_ADDRESS_REQ: programs own individual address
 *                                   into chip for auto-ACK of unicast frames)
 *     Host  ->  0x02 (U_STATE_REQ)
 *
 *   Transmitting a frame (N bytes including checksum):
 *     For each byte at position i (0-indexed):
 *       If last byte (i == N-1):  send  (0x40 | i)  then  frame[i]
 *       Otherwise:                send  (0x80 | i)  then  frame[i]
 *     After all bytes: wait for L_DATA_CON from chip (bit7 set = success).
 *
 *   Receiving a frame:
 *     The chip sends the KNX ctrl1 byte as the first byte.  For standard frames,
 *     (ctrl1 & 0xD3) == 0x90 (L_DATA_STANDARD_IND).  This byte is ALSO the ctrl1
 *     of the KNX frame — it must be kept as frame[0].
 *     Frame length is determined from byte 5 (DAF byte, bits 3-0 = APDU_len - 1).
 *     When the frame is complete, the host sends U_ACK_REQ to the chip.
 *
 *   Status bytes sent by chip at any time:
 *     0x03               U_RESET_IND     chip was reset (reconnect needed)
 *     byte & 0x07 == 7   U_STATE_IND     error flags (collision, overtemp, etc.)
 *     byte & 0x7F == 0x0B  L_DATA_CON   TX confirmation (bit7: 1=ok, 0=fail)
 *
 * ACK mechanism:
 *   At frame start:    send 0x10 (U_ACK_REQ, no flags = not-addressed)
 *   After 7 bytes:     if destination address matches own_addr or any registered group:
 *                        send 0x11 (U_ACK_REQ | U_ACK_REQ_ADRESSED = positive ACK)
 *
 * Usage:
 *   knx_tp_init(own_addr);
 *   knx_tp_set_rx_callback(my_callback);
 *   // In loop():
 *   knx_tp_poll();
 */

/* TP-UART host → chip command bytes */
#define TPUART_RESET_REQ        0x01u
#define TPUART_STATE_REQ        0x02u
#define TPUART_SET_ADDR_REQ     0xF1u   /* followed by 2 address bytes */
#define TPUART_ACK_REQ          0x10u
#define TPUART_ACK_ADDRESSED    0x01u   /* OR into ACK_REQ for positive ACK */
#define TPUART_TX_START         0x80u   /* OR with position for non-last bytes */
#define TPUART_TX_END           0x40u   /* OR with position for last byte */

/* TP-UART chip → host indicator bytes / masks */
#define TPUART_RESET_IND        0x03u
#define TPUART_DATA_STD_MASK    0xD3u
#define TPUART_DATA_STD_IND     0x90u   /* standard frame ctrl1 & mask == this */
#define TPUART_DATA_EXT_IND     0x10u   /* extended frame ctrl1 & mask == this */
#define TPUART_STATE_MASK       0x07u
#define TPUART_STATE_IND        0x07u
#define TPUART_CON_MASK         0x7Fu
#define TPUART_CON_IND          0x0Bu
#define TPUART_CON_SUCCESS      0x80u   /* bit 7 set = TX confirmed OK */

typedef void (*knx_tp_rx_cb_t)(const KnxTelegram *tg);

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize the TP transport.
 * Configures UART and GPIO, then performs the STKNX reset handshake and programs
 * own_addr into the chip for auto-ACK of unicast frames.
 * Returns true if the chip responded with U_RESET_IND within 10 ms. */
bool knx_tp_init(KnxIndividualAddr own_addr);

/* Register a callback invoked for each received, valid telegram. */
void knx_tp_set_rx_callback(knx_tp_rx_cb_t cb);

/* Transmit a telegram onto the KNX TP bus using the slot-byte TX protocol.
 * Returns false if the chip is not connected or the frame cannot be serialized. */
bool knx_tp_send(const KnxTelegram *tg);

/* Receive poll — call every iteration of loop().
 * Drives the byte-by-byte RX state machine, sends ACKs, handles TX confirmation,
 * and delivers complete valid telegrams to the registered callback.
 * Also drives programming key debounce. */
void knx_tp_poll(void);

/* Return true if the chip has responded to reset and is ready. */
bool knx_tp_connected(void);

/* Return true if the STKNX bus-OK signal (KNX_TP_OK_PIN) is asserted. */
bool knx_tp_bus_ok(void);

/* Return true if the STKNX power-OK signal (KNX_TP_VCC_OK_PIN) is asserted. */
bool knx_tp_vcc_ok(void);

/* Programming mode LED control. */
void knx_tp_prog_led_set(bool on);

/* Return true if the programming pushbutton is pressed (debounced). */
bool knx_tp_prog_key_pressed(void);

#ifdef __cplusplus
}
#endif
