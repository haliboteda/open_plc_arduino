/*
 * openplc_reset.h -- why the board last reset, for the sketch.
 * The bootloader reads and clears RCC->RSR before it jumps, then publishes the
 * value in SRAM4 (IAP_boot_handoff.h). Decision 80; sequence in
 * $PROD/docs/modules/M1/BOOT-SEQUENCE.md.
 */
#ifndef OPENPLC_RESET_H_
#define OPENPLC_RESET_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  OPENPLC_RESET_UNKNOWN = 0,  /* nothing published, or a cause not listed here */
  OPENPLC_RESET_POWER_ON,
  OPENPLC_RESET_PIN,          /* the reset button / NRST */
  OPENPLC_RESET_SOFTWARE,     /* NVIC_SystemReset, e.g. after an upload */
  OPENPLC_RESET_WATCHDOG,     /* IWDG or WWDG */
  OPENPLC_RESET_BROWNOUT,
} openplc_reset_cause_t;

/* Decodes RCC->RSR bits. A watchdog also pulses NRST, so its flag wins over
 * the pin flag; same order as the bootloader's boot log. */
openplc_reset_cause_t openplc_reset_cause_from_rsr(uint32_t rsr);

/* The cause the bootloader published for this boot. */
openplc_reset_cause_t openplc_reset_cause(void);

#ifdef __cplusplus
}
#endif

#endif /* OPENPLC_RESET_H_ */
