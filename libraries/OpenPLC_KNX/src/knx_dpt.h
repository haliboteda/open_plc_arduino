#pragma once

#include <stdint.h>
#include <stdbool.h>

/*
 * KNX Data Point Type (DPT) encoding and decoding.
 *
 * DPTs are the standardized data representation used across all KNX devices.
 * They are NOT IEEE 754 floats — the KNX 2-byte float (DPT-9) uses its own
 * sign/exponent/mantissa format specified in KNX spec Part 3.7.2.
 *
 * Supported DPTs:
 *   DPT-1.x   1 bit    bool       (on/off, open/close, enable/disable …)
 *   DPT-5.x   1 byte   uint8      (percentage 0-100%, scene number …)
 *   DPT-9.x   2 bytes  KNX float  (temperature, illuminance, wind speed …)
 *   DPT-14.x  4 bytes  IEEE 754   (physical values with high precision)
 *
 * Encoding functions write into a caller-supplied byte buffer.
 * Decoding functions read from a byte buffer and return the decoded value.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* --- DPT-1: 1-bit boolean ------------------------------------------------ */

/* Encode a boolean value into a 1-byte buffer.
 * Wire format: 0x01 = true, 0x00 = false.
 * (The library will pack this into the APCI short-data field.) */
void     dpt1_encode(bool value, uint8_t *out);
bool     dpt1_decode(const uint8_t *data, uint8_t len);

/* --- DPT-5: 1-byte unsigned integer -------------------------------------- */

/* Encode/decode an unsigned byte value (e.g., 0-255 for DPT-5.010 percentage). */
void     dpt5_encode(uint8_t value, uint8_t *out);
uint8_t  dpt5_decode(const uint8_t *data, uint8_t len);

/* --- DPT-9: KNX 2-byte floating-point ------------------------------------ */

/*
 * KNX float wire format (KNX spec 3.7.2.3.10):
 *
 *   Byte 0: S E E E E M M M
 *   Byte 1: M M M M M M M M
 *
 *   Value = 0.01 × M × 2^E
 *   M: signed 11-bit mantissa (two's complement)
 *   E: unsigned 4-bit exponent
 *   S: sign bit (= sign of M, also byte0 bit 7)
 *
 * Representable range: approx. −671 088.64 to +670 760.96
 * Resolution at E=0: 0.01 (two decimal places)
 *
 * Returns false from dpt9_encode if the value is out of the encodable range.
 */
bool     dpt9_encode(float value, uint8_t out[2]);
float    dpt9_decode(const uint8_t data[2]);

/* --- DPT-14: 4-byte IEEE 754 float --------------------------------------- */

void     dpt14_encode(float value, uint8_t out[4]);
float    dpt14_decode(const uint8_t data[4]);

#ifdef __cplusplus
}
#endif
