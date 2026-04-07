#pragma once

/*
 * Compile-time configuration for the OpenPLC_KNX library.
 *
 * Override any of these macros before including OpenPLC_KNX.h, or pass them
 * as compiler flags (e.g. -DKNX_MAX_GROUP_OBJECTS=64).
 *
 * Pin macros reference the values defined in variant_PLC_H743.h.  If this
 * library is used on a different board the relevant macros must be redefined.
 */

#include <Arduino.h>

/* ---------- KNX TP transport (STKNX transceiver via USART1) ----------
 *
 * Hardware path:  STM32H743 USART1 <-> TLP2362 optocoupler <-> STKNX (U9) <-> KNX bus
 *
 * STKNX uses the standard TP-UART2 protocol (same command byte set as NCN5120).
 * UART: 19200 bps, 8 data bits, even parity, 1 stop bit (SERIAL_8E1).
 *
 * NOTE: KNX_TP_SERIAL is NOT defined here as a global like 'Serial1' because
 * HAVE_HWSERIAL1 is not enabled in the OpenPLC variant and Serial1 would be an
 * undefined reference at link time.  Instead, knx_tp.cpp declares a private
 * static HardwareSerial(USART1) instance and defines KNX_TP_SERIAL to refer to it.
 */
#ifndef KNX_TP_BAUD
#  define KNX_TP_BAUD           19200
#endif
#ifndef KNX_TP_SERIAL_CONFIG
#  define KNX_TP_SERIAL_CONFIG  SERIAL_8E1  /* 8 data bits, even parity, 1 stop bit */
#endif

/* STKNX status and control GPIO pins (from Bridge MPU schematic) */
#ifndef KNX_TP_OK_PIN
#  define KNX_TP_OK_PIN         PD7     /* STKNX pin 21: high = KNX bus operational */
#endif
#ifndef KNX_TP_VCC_OK_PIN
#  define KNX_TP_VCC_OK_PIN     PH12    /* STKNX pin 19: high = STKNX power OK */
#endif
#ifndef KNX_TP_PROG_KEY_PIN
#  define KNX_TP_PROG_KEY_PIN   PG9     /* Programming mode pushbutton (active low) */
#endif
#ifndef KNX_TP_PROG_LED_PIN
#  define KNX_TP_PROG_LED_PIN   PG11    /* Programming mode LED (active high) */
#endif

/* ---------- KNXnet/IP transport (UDP multicast over Ethernet/LwIP) ---------- */
#ifndef KNX_IP_PORT
#  define KNX_IP_PORT           3671
#endif
#ifndef KNX_IP_MCAST_STR
#  define KNX_IP_MCAST_STR      "224.0.23.12"
#endif

/* ---------- Group object table ---------- */
#ifndef KNX_MAX_GROUP_OBJECTS
#  define KNX_MAX_GROUP_OBJECTS 32
#endif

/* ---------- Protocol constants ---------- */
/* Maximum APDU payload length (KNX standard: up to 14 data bytes) */
#define KNX_APDU_MAX_DATA_LEN   14
/* Full APDU buffer = TPCI/APCI (2 bytes) + data (up to 14 bytes) */
#define KNX_APDU_BUF_LEN        (2 + KNX_APDU_MAX_DATA_LEN)

/* TP receive ring buffer size (bytes) */
#define KNX_TP_RX_BUF_SIZE      128

/* Inter-byte timeout for TP frame boundary detection (ms).
 * KNX spec requires a gap of >= 20 bit-times between frames.
 * At 19200 bps that is ~1.0 ms; we use a conservative 5 ms. */
#define KNX_TP_FRAME_TIMEOUT_MS 5

/* Programming pushbutton debounce period (ms) */
#define KNX_PROG_DEBOUNCE_MS    50
