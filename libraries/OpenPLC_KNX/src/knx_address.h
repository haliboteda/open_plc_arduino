#pragma once

#include <stdint.h>

/*
 * KNX address types and helpers.
 *
 * KNX Individual Address (IA):  area(4 bits).line(4 bits).device(8 bits)
 *   packed into a 16-bit big-endian value.
 *
 * KNX Group Address (GA), 3-level encoding:
 *   main(5 bits)/middle(3 bits)/sub(8 bits)
 *   packed into a 16-bit big-endian value.
 */

typedef uint16_t KnxIndividualAddr;
typedef uint16_t KnxGroupAddr;

/* Build an individual address from area, line, and device numbers. */
static inline KnxIndividualAddr knxIA(uint8_t area, uint8_t line, uint8_t device)
{
    return (KnxIndividualAddr)(((uint16_t)(area   & 0x0Fu) << 12)
                             | ((uint16_t)(line   & 0x0Fu) <<  8)
                             |  (uint16_t)(device));
}

/* Build a 3-level group address from main, middle, and sub fields. */
static inline KnxGroupAddr knxGA(uint8_t main, uint8_t middle, uint8_t sub)
{
    return (KnxGroupAddr)(((uint16_t)(main   & 0x1Fu) << 11)
                        | ((uint16_t)(middle & 0x07u) <<  8)
                        |  (uint16_t)(sub));
}

/* Decompose an individual address back into its fields. */
static inline uint8_t knxIA_area  (KnxIndividualAddr ia) { return (uint8_t)((ia >> 12) & 0x0Fu); }
static inline uint8_t knxIA_line  (KnxIndividualAddr ia) { return (uint8_t)((ia >>  8) & 0x0Fu); }
static inline uint8_t knxIA_device(KnxIndividualAddr ia) { return (uint8_t)(ia & 0xFFu); }

/* Decompose a 3-level group address back into its fields. */
static inline uint8_t knxGA_main  (KnxGroupAddr ga) { return (uint8_t)((ga >> 11) & 0x1Fu); }
static inline uint8_t knxGA_middle(KnxGroupAddr ga) { return (uint8_t)((ga >>  8) & 0x07u); }
static inline uint8_t knxGA_sub   (KnxGroupAddr ga) { return (uint8_t)(ga & 0xFFu); }
