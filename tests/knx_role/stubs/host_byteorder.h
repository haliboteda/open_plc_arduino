/* Byte-order helpers for the host build: knx/bits.h only provides them on the
 * board (via lwIP) and on Linux. The host is little-endian like the board. */
#pragma once
#include <stdint.h>
#ifndef htons
static inline uint16_t host_bswap16(uint16_t x) { return (uint16_t)((x << 8) | (x >> 8)); }
static inline uint32_t host_bswap32(uint32_t x)
{
    return (x << 24) | ((x << 8) & 0x00FF0000u) | ((x >> 8) & 0x0000FF00u) | (x >> 24);
}
#define htons(x) host_bswap16((uint16_t)(x))
#define ntohs(x) host_bswap16((uint16_t)(x))
#define htonl(x) host_bswap32((uint32_t)(x))
#define ntohl(x) host_bswap32((uint32_t)(x))
#endif
