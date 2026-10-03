/* Force-included first. IAP_boot_handoff.c's "stm32_def.h" resolves to the real
 * header next to it (a quoted include searches the source's own directory
 * before -I), so pull in the stub here and mark the real one as already seen.
 * SRAM4's first 32 bytes become a RAM buffer. */
#include <stdint.h>
#include "stm32_def.h"
#define _STM32_DEF_
extern uint32_t fake_sram4[8];
#define BOOT_HANDOFF_ADDR ((uintptr_t)fake_sram4)
