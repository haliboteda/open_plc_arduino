#include "knx_dpt.h"
#include <string.h>
#include <math.h>

/* -------------------------------------------------------------------------
 * DPT-1: 1-bit boolean
 * ---------------------------------------------------------------------- */

void dpt1_encode(bool value, uint8_t *out)
{
    out[0] = value ? 0x01u : 0x00u;
}

bool dpt1_decode(const uint8_t *data, uint8_t len)
{
    if (len == 0u) {
        return false;
    }
    return (data[0] & 0x01u) != 0u;
}

/* -------------------------------------------------------------------------
 * DPT-5: 1-byte unsigned integer
 * ---------------------------------------------------------------------- */

void dpt5_encode(uint8_t value, uint8_t *out)
{
    out[0] = value;
}

uint8_t dpt5_decode(const uint8_t *data, uint8_t len)
{
    if (len == 0u) {
        return 0u;
    }
    return data[0];
}

/* -------------------------------------------------------------------------
 * DPT-9: KNX 2-byte floating-point
 *
 * Encoding algorithm:
 *  1. Compute mantissa M = round(value × 100)  (value in base unit, e.g. °C)
 *  2. Set exponent E = 0
 *  3. While M > 2047 or M < -2048: right-shift M by 1, increment E
 *  4. If E > 15: overflow — clamp to max representable value
 *  5. Pack:
 *       byte[0] = sign(M)<<7 | E[3:0]<<3 | M[10:8]
 *       byte[1] = M[7:0]
 *
 * The C right-shift of a signed negative integer is arithmetic on ARM/GCC.
 * ---------------------------------------------------------------------- */

bool dpt9_encode(float value, uint8_t out[2])
{
    /* Resolution: 0.01 per step at E=0 */
    int32_t m = (int32_t)roundf(value * 100.0f);
    uint8_t e = 0u;

    /* Scale mantissa to fit 11-bit signed range [-2048, 2047] */
    while (m > 2047 || m < -2048) {
        m >>= 1;    /* arithmetic right shift */
        e++;
        if (e > 15u) {
            /* Value out of DPT-9 range — saturate to maximum */
            if (value >= 0.0f) {
                out[0] = 0x7Fu; out[1] = 0xFFu;
            } else {
                out[0] = 0xF8u; out[1] = 0x00u;
            }
            return false;
        }
    }

    /* Pack the two wire bytes:
     *   byte[0]: S  E3 E2 E1 E0  M10 M9 M8
     *   byte[1]: M7 M6 M5 M4 M3  M2  M1 M0  */
    out[0] = (uint8_t)(((m < 0) ? 0x80u : 0x00u)
                      | ((e & 0x0Fu) << 3)
                      | ((m >> 8) & 0x07u));
    out[1] = (uint8_t)(m & 0xFFu);
    return true;
}

float dpt9_decode(const uint8_t data[2])
{
    /* Extract exponent */
    uint8_t  e       = (data[0] >> 3) & 0x0Fu;
    /* Extract 11-bit mantissa (sign extended to int16_t) */
    uint16_t m_raw   = (uint16_t)(((uint16_t)(data[0] & 0x07u) << 8) | data[1]);
    int16_t  m;
    /* Sign-extend from bit 10 */
    if (m_raw & 0x0400u) {
        m = (int16_t)(m_raw | 0xF800u);
    } else {
        m = (int16_t)m_raw;
    }
    return 0.01f * (float)m * (float)(1u << e);
}

/* -------------------------------------------------------------------------
 * DPT-14: 4-byte IEEE 754 float (big-endian on the wire)
 * ---------------------------------------------------------------------- */

void dpt14_encode(float value, uint8_t out[4])
{
    /* Reinterpret float bits as uint32_t, then store big-endian */
    uint32_t bits;
    memcpy(&bits, &value, 4u);
    out[0] = (uint8_t)(bits >> 24);
    out[1] = (uint8_t)(bits >> 16);
    out[2] = (uint8_t)(bits >>  8);
    out[3] = (uint8_t)(bits);
}

float dpt14_decode(const uint8_t data[4])
{
    uint32_t bits = ((uint32_t)data[0] << 24)
                  | ((uint32_t)data[1] << 16)
                  | ((uint32_t)data[2] <<  8)
                  |  (uint32_t)data[3];
    float value;
    memcpy(&value, &bits, 4u);
    return value;
}
