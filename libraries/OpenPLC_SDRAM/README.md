# OpenPLC_SDRAM

中文：[README.zh-CN.md](README.zh-CN.md)

Use the board's 64 MB external SDRAM (AS4C32M16SB, on the FMC bus at
`0xC0000000`) from an application.

```cpp
#include <OpenPLC_SDRAM.h>

void setup() {
  if (!SDRAM.begin()) {
    // no usable SDRAM -- decide what that means for this application
  }
  uint16_t *samples = (uint16_t *)SDRAM.alloc(2 * 1024 * 1024);  // already zeroed
}
```

Start with the `SDRAM_Basic` example; `SDRAM_DataLogger` shows the case the
memory exists for — a history buffer far larger than the 512 KB of internal RAM.

## API

| Call | Does |
|---|---|
| `SDRAM.begin()` | Powers up the controller and the chip, checks the memory answers. Safe to call repeatedly. **Returns `false` if the memory is unusable.** |
| `SDRAM.ready()` | Whether `begin()` has succeeded |
| `SDRAM.alloc(bytes, align = 8)` | Returns `bytes` of **zeroed** memory, or `nullptr` |
| `SDRAM.allocUninitialized(bytes, align = 8)` | Same without the zeroing — the buffer holds whatever was there before |
| `SDRAM.available()` / `SDRAM.used()` | Bytes left / handed out |
| `OpenPLC_SDRAM_Class::CAPACITY` | 64 MiB |

## Three things to know

**1. There is no `free()`.** Allocation only moves forward. Ask for what you
need once, in `setup()`, and keep it for the life of the program. A PLC does not
need a heap here, and a heap would add fragmentation and a new way to fail.

**2. Zeroing is not free.** Measured on this board: **91 MB/s**, so 1 MB costs
about 11 ms and the full 64 MB about 700 ms. That is fine in `setup()` and not
fine in `loop()`. If a large buffer is going to be overwritten immediately
anyway, `allocUninitialized()` skips it — the ugly name is deliberate, because
plausible-looking uninitialised data is exactly the bug this library removes.

**3. SDRAM does not survive a power cut.** It is working memory, not storage.
Anything that must outlive a power-down belongs in flash or on the SD card.

## How fast it is

Measured 2026-10-14 on the board, as shipped (480 MHz, D-cache off, MPU off),
MiB/s; method in `OpenPLC_Test/onboard/sdram/RAM_Speed`.

| | DTCM | AXI SRAM (normal variables) | SDRAM |
|---|---|---|---|
| 32-bit sequential write | 1331 | 1046 | 223 |
| 32-bit sequential read | 1331 | 179 | 62 |
| `memcpy` | 189 | 43 | **4.4** |

SDRAM is about 3x slower than a normal variable for reads, and `memcpy` into or
out of it is very slow, because the shipped `memcpy` copies byte by byte and SDRAM
is not cached. To move bulk data, copy 32-bit words in a loop. Turning the
D-cache on does not speed SDRAM up (with the MPU off it is uncached).

## Why you cannot declare a variable "in SDRAM"

There is no `__attribute__((section(".sdram_bss")))` to put a big array
directly in SDRAM. That was the original design and it was dropped, because it
hands you four ways to be wrong and three of them are silent:

- the section must be `NOLOAD`, or a 1 MB array adds 1 MB to the firmware image;
- it must sit outside `_sbss.._ebss`, or the startup zeroing loop writes to
  SDRAM *before the controller exists* and the board faults;
- it must have no initialiser, for the same reason via the `.data` copy;
- and the memory is **not zeroed** — the array holds whatever survived the last
  power-down, so the program looks correct most of the time.

An address you can only get from `alloc()` cannot be touched before the
controller is up, because `alloc()` returns `nullptr` until `begin()` has
succeeded. The traps stop existing instead of being documented.

## If `begin()` returns false

The read-back check failed: the memory is not answering correctly. `alloc()`
then returns `nullptr` rather than handing out addresses that silently lose
data. What to do about it is the application's call — a data logger might carry
on with a shorter in-SRAM history, while something that needs the buffer to
function should say so and stop.

When the board stays in upload mode, the bootloader runs the same check and
writes either `SDRAM staging buffer OK` or `** SDRAM SELF-TEST FAILED **` to
its boot log (lines starting `[sys][BL]`), which shows on USB port 1 (`Serial`)
once the computer opens it. That is the quickest way to tell a board problem
from a sketch problem.
