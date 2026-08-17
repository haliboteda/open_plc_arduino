/*
 * OpenPLC_SDRAM -- the 64 MB external SDRAM, wrapped.
 *
 * The board carries an AS4C32M16SB (64 MiB) on the FMC bus at 0xC0000000. This
 * library is the only supported way for an application to use it.
 *
 *     void setup() {
 *       if (!SDRAM.begin()) {
 *         // no usable SDRAM -- decide what that means for this application
 *       }
 *       uint16_t *samples = (uint16_t *)SDRAM.alloc(2 * 1024 * 1024);
 *     }
 *
 * ---------------------------------------------------------------------------
 * Why there is no way to declare a variable "in SDRAM"
 * ---------------------------------------------------------------------------
 * The obvious alternative is a linker section, so a user could write
 *
 *     uint8_t big[1000000] __attribute__((section(".sdram_bss")));
 *
 * That was the original plan and it was dropped on purpose. It hands the user
 * four ways to be wrong, three of them silent:
 *
 *   - the section must be NOLOAD, or the array lands in the .bin and a 1 MB
 *     buffer costs 1 MB of firmware image;
 *   - it must sit outside _sbss.._ebss, or the startup zeroing loop writes to
 *     SDRAM before the controller exists and the board faults;
 *   - it must have no initialiser, for the same reason via the .data copy;
 *   - and the memory is not zeroed, so the array holds whatever survived the
 *     last power-down. That one is the worst: the program looks correct most
 *     of the time.
 *
 * An address you can only obtain from alloc() cannot be touched before the
 * controller is up, because alloc() fails until begin() has run. The four
 * traps stop existing rather than being documented.
 *
 * ---------------------------------------------------------------------------
 * Allocation is one-way
 * ---------------------------------------------------------------------------
 * There is no free(). A PLC application allocates its buffers once at startup
 * and keeps them for the life of the program; giving it a heap here would add
 * fragmentation and a failure mode for no use case anybody asked for. If that
 * changes, a real allocator can be added later without breaking this API --
 * the reverse is not true.
 */

#ifndef OPENPLC_SDRAM_H
#define OPENPLC_SDRAM_H

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

class OpenPLC_SDRAM_Class {
  public:
    /* Physical window. SDNE0 maps bank 1 here; see the bootloader's
     * Core/Src/fmc.c, which configures the same chip the same way. */
    static const uint32_t BASE = 0xC0000000UL;
    static const uint32_t CAPACITY = 64UL * 1024UL * 1024UL;

    /*
     * Bring the controller up, run the SDRAM's power-up sequence and check the
     * memory answers. Safe to call more than once; the work happens once.
     *
     * Returns false if the read-back check fails, which means the memory is
     * not usable -- alloc() then returns nullptr rather than handing out
     * addresses that silently lose data.
     */
    bool begin();

    /* Whether begin() has succeeded. */
    bool ready() const { return _ready; }

    /*
     * Hand out `bytes` of zeroed SDRAM, aligned to `align` (8 by default,
     * enough for any type this core has). Returns nullptr when begin() has not
     * succeeded or the request does not fit.
     *
     * ⚠️ Zeroing 64 MB is not instant. Allocate in setup(), not in loop().
     * If a large buffer is about to be overwritten anyway, see
     * allocUninitialized().
     */
    void *alloc(size_t bytes, size_t align = 8);

    /*
     * Same, but skips the zeroing. The buffer holds whatever was in those
     * cells before -- usually the previous run's data.
     *
     * The name is deliberately unpleasant: uninitialised memory that looks
     * plausible is the failure mode this library exists to remove, so opting
     * back into it has to be visible at the call site.
     */
    void *allocUninitialized(size_t bytes, size_t align = 8);

    /* Bytes not yet handed out. */
    size_t available() const { return _ready ? (CAPACITY - _used) : 0; }

    /* Bytes handed out so far. */
    size_t used() const { return _used; }

  private:
    bool selfTest();

    bool _ready = false;
    size_t _used = 0;
};

extern OpenPLC_SDRAM_Class SDRAM;

#endif /* OPENPLC_SDRAM_H */
