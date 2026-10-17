/* Force-included first. boot_log.c's "stm32_def.h" would resolve to the real
 * header next to it, so pull in the stub here and mark the real one as seen.
 * The SRAM4 boot-log area becomes a RAM buffer. */
#include <stdint.h>
#include "stm32_def.h"
#define _STM32_DEF_
extern uint32_t fake_sram4_log[1024];
#define BOOT_LOG_ADDR ((uintptr_t)fake_sram4_log)
