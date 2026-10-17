/*
 * openplc_reset.c -- see openplc_reset.h.
 */
#include "openplc_reset.h"
#include "stm32_def.h"
#include "IAP_boot_handoff.h"

/* A watchdog also pulses NRST, so its flag wins over the pin flag; same order
 * as the bootloader's boot log. */
static openplc_reset_cause_t reset_cause_from_rsr(uint32_t rsr)
{
  if ((rsr & (RCC_RSR_IWDG1RSTF | RCC_RSR_WWDG1RSTF)) != 0U) {
    return OPENPLC_RESET_WATCHDOG;
  }
  if ((rsr & RCC_RSR_SFTRSTF) != 0U) {
    return OPENPLC_RESET_SOFTWARE;
  }
  /* Power-on also sets the pin and brownout flags, so it is checked first. */
  if ((rsr & RCC_RSR_PORRSTF) != 0U) {
    return OPENPLC_RESET_POWER_ON;
  }
  if ((rsr & RCC_RSR_PINRSTF) != 0U) {
    return OPENPLC_RESET_PIN;
  }
  if ((rsr & RCC_RSR_BORRSTF) != 0U) {
    return OPENPLC_RESET_BROWNOUT;
  }
  return OPENPLC_RESET_UNKNOWN;
}

openplc_reset_cause_t openplcResetCause(void)
{
  uint32_t rsr;

  if (!boot_handoff_published_reset_rsr(&rsr)) {
    return OPENPLC_RESET_UNKNOWN;
  }
  return reset_cause_from_rsr(rsr);
}
