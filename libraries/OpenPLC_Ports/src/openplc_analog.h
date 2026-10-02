/*
 * openplc_analog.h -- AI / AO in mV and mA with this board's calibration.
 * Design: $PROD/docs/modules/M3/CALIBRATED-ANALOG.md.
 */

#ifndef OPENPLC_ANALOG_H_
#define OPENPLC_ANALOG_H_

#include <stdint.h>
#include "openplc_calib.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OPENPLC_ADC_FULL   4095U
#define OPENPLC_AO_MAX_MA  20.0f

/* Nominal conversion, then gain * nominal + offset. raw is a 12-bit reading. */
float openplc_ai1_mv_from_raw(uint32_t raw, const calib_channel_t *c);
float openplc_ai2_ma_from_raw(uint32_t raw, const calib_channel_t *c);

/* 12-bit DAC code that makes the output carry ma after calibration: the
 * hardware is given (target - offset) / gain. ma is clamped to 0..20 mA. */
uint32_t openplc_ao_code_from_ma(float ma, const calib_channel_t *c);

/* The calibration in force, read from flash on the first call only. When it is
 * not valid the coefficients are nominal and one line is logged, once. */
const calib_area_t *openplc_calib_get(calib_status_t *status);

#ifdef __cplusplus
}
#endif

#endif /* OPENPLC_ANALOG_H_ */
