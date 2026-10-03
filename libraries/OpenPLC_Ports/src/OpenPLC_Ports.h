/*
 * OpenPLC_Ports -- helpers the port examples share.
 */

#ifndef OPENPLC_PORTS_H_
#define OPENPLC_PORTS_H_

#include <Arduino.h>

/* Full scale of analogRead()/analogWrite() once openplcEnableVref() has run. */
#define OPENPLC_VREF_MV 2500U

/* Turns on the chip's internal 2.5 V reference. The board has no external one,
 * so analogRead() and the DAC mean nothing without it. See
 * $PROD/docs/hardware/HARDWARE-FACTS.md, "模拟量：没有外部基准". Returns false if
 * the reference did not come up within 10 ms. */
bool openplcEnableVref(void);

#include "openplc_calib.h"

/* AI / AO in mV and mA with this board's calibration; nominal conversion and
 * one log line when the calibration is not valid. They enable the reference
 * themselves and leave the ADC and DAC at 12 bits. See
 * $PROD/docs/modules/M3/CALIBRATED-ANALOG.md. */
float openplcReadAI1_mV(void);
float openplcReadAI2_mA(void);
/* channel 1 = AO1, 2 = AO2; mA is clamped to 0..20. */
void openplcWriteAO_mA(uint8_t channel, float mA);
calib_status_t openplcCalibrationStatus(void);

#include "openplc_reset.h"

/* Why the board last reset: watchdog, power-on, reset button, software. Use it
 * at the top of setup() to decide what to do after a watchdog reset; the board
 * package decides nothing on its own (decision 80). Upstream
 * IWatchdog::isReset() always reads false on this board: the bootloader has
 * already cleared the flag it looks at. */
openplc_reset_cause_t openplcResetCause(void);

#endif /* OPENPLC_PORTS_H_ */
