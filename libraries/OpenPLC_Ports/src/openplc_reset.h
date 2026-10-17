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

/* Why the board last reset: watchdog, power-on, reset button, software. Use it
 * at the top of setup() to decide what to do after a watchdog reset; the board
 * package decides nothing on its own (decision 80). Upstream
 * IWatchdog::isReset() always reads false on this board: the bootloader has
 * already cleared the flag it looks at. */
openplc_reset_cause_t openplcResetCause(void);

#ifdef __cplusplus
}
#endif

#endif /* OPENPLC_RESET_H_ */
