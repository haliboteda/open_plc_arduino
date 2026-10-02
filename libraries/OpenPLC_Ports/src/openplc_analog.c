#include "openplc_analog.h"
#include <stdio.h>

/* Overridable so the host test (T3-07) can count the lines. */
#ifndef OPENPLC_LOG_LINE
#define OPENPLC_LOG_LINE(msg) printf("%s\n", (msg))
#endif

/* Pin voltage in mV for a 12-bit reading against the internal 2.5 V reference. */
static float pin_mv(uint32_t raw)
{
	return (float)raw * 2500.0f / (float)OPENPLC_ADC_FULL;
}

float openplc_ai1_mv_from_raw(uint32_t raw, const calib_channel_t *c)
{
	/* 90.6k / 22.6k divider in front of the ADC pin. */
	float nominal = pin_mv(raw) * (90.6f / 22.6f);
	return c->gain * nominal + c->offset;
}

float openplc_ai2_ma_from_raw(uint32_t raw, const calib_channel_t *c)
{
	/* 124 R shunt. */
	float nominal = pin_mv(raw) / 124.0f;
	return c->gain * nominal + c->offset;
}

uint32_t openplc_ao_code_from_ma(float ma, const calib_channel_t *c)
{
	if (ma < 0.0f) {
		ma = 0.0f;
	} else if (ma > OPENPLC_AO_MAX_MA) {
		ma = OPENPLC_AO_MAX_MA;
	}
	float nominal = (c->gain != 0.0f) ? (ma - c->offset) / c->gain : ma;
	/* XTR111: Iout = Vin * 10 / 1024 R, so Vin = mA * 102.4 mV. */
	float code = nominal * 102.4f * (float)OPENPLC_ADC_FULL / 2500.0f + 0.5f;
	if (code <= 0.0f) {
		return 0U;
	}
	if (code >= (float)OPENPLC_ADC_FULL) {
		return OPENPLC_ADC_FULL;
	}
	return (uint32_t)code;
}

const calib_area_t *openplc_calib_get(calib_status_t *status)
{
	static calib_area_t area;
	static calib_status_t st;
	static int loaded;

	if (!loaded) {
		st = openplc_calib_load(&area);
		loaded = 1;
		if (st != CALIB_OK) {
			OPENPLC_LOG_LINE(st == CALIB_BLANK ? "OpenPLC_Ports: no calibration on this board, using nominal AI/AO conversion"
			                 : st == CALIB_CORRUPT ? "OpenPLC_Ports: calibration area is corrupt, using nominal AI/AO conversion"
			                 : "OpenPLC_Ports: calibration belongs to another board, using nominal AI/AO conversion");
		}
	}
	if (status != NULL) {
		*status = st;
	}
	return &area;
}
