/* Host stand-in for the core's stm32_def.h: RCC->RSR, its flag bits (RM0433
 * RCC_RSR) and the cache/barrier calls IAP_boot_handoff.c makes. */
#ifndef STM32_DEF_H_STUB
#define STM32_DEF_H_STUB

#include <stdint.h>

typedef struct { volatile uint32_t RSR; } RCC_TypeDef;
extern RCC_TypeDef fake_rcc;
#define RCC (&fake_rcc)

#define RCC_RSR_WWDG1RSTF (1UL << 28)
#define RCC_RSR_IWDG1RSTF (1UL << 26)
#define RCC_RSR_SFTRSTF   (1UL << 24)
#define RCC_RSR_PORRSTF   (1UL << 23)
#define RCC_RSR_PINRSTF   (1UL << 22)
#define RCC_RSR_BORRSTF   (1UL << 21)

/* Writing RMVF clears every flag: what the bootloader does on every boot. */
#define __HAL_RCC_CLEAR_RESET_FLAGS() (fake_rcc.RSR = 0U)

static inline void __DSB(void) {}
static inline void __ISB(void) {}
static inline void SCB_CleanDCache_by_Addr(uint32_t *a, int32_t n) { (void)a; (void)n; }
static inline void SCB_InvalidateDCache_by_Addr(uint32_t *a, int32_t n) { (void)a; (void)n; }
void HAL_NVIC_SystemReset(void);

#endif
