/* Host stand-in for the core's stm32_def.h: the interrupt-mask, barrier and
 * cache calls boot_log.c makes. PRIMASK is a plain variable so the test can
 * check it is restored. */
#ifndef STM32_DEF_H_STUB
#define STM32_DEF_H_STUB

#include <stdint.h>

extern uint32_t fake_primask;

static inline uint32_t __get_PRIMASK(void) { return fake_primask; }
static inline void __set_PRIMASK(uint32_t v) { fake_primask = v; }
static inline void __disable_irq(void) { fake_primask = 1U; }
static inline void __DSB(void) {}
static inline void SCB_CleanDCache_by_Addr(uint32_t *a, int32_t n) { (void)a; (void)n; }

#endif
