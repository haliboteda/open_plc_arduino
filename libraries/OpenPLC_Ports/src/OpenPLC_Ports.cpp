/*
 * OpenPLC_Ports.cpp -- see OpenPLC_Ports.h.
 */

#include "OpenPLC_Ports.h"
#include "openplc_analog.h"

/* Reference on and calibration read, once, before the first conversion. */
static const calib_area_t *analog_ready(void)
{
  static bool vref_on;

  if (!vref_on) {
    vref_on = openplcEnableVref();
  }
  return openplc_calib_get(NULL);
}

float openplcReadAI1_mV(void)
{
  const calib_area_t *cal = analog_ready();
  analogReadResolution(12);
  return openplc_ai1_mv_from_raw(analogRead(AIN_1), &cal->ch[CALIB_CH_AI1]);
}

float openplcReadAI2_mA(void)
{
  const calib_area_t *cal = analog_ready();
  analogReadResolution(12);
  return openplc_ai2_ma_from_raw(analogRead(AIN_2), &cal->ch[CALIB_CH_AI2]);
}

void openplcWriteAO_mA(uint8_t channel, float mA)
{
  if ((channel != 1U) && (channel != 2U)) {
    return;
  }
  const calib_area_t *cal = analog_ready();
  const calib_channel_t *c = &cal->ch[(channel == 1U) ? CALIB_CH_AO1 : CALIB_CH_AO2];
  analogWriteResolution(12);
  analogWrite((channel == 1U) ? AOUT_1 : AOUT_2, openplc_ao_code_from_ma(mA, c));
}

calib_status_t openplcCalibrationStatus(void)
{
  calib_status_t st;
  (void)openplc_calib_get(&st);
  return st;
}

bool openplcEnableVref(void)
{
  /* VREFBUF has its own clock bit; without it writes to CSR are dropped. */
  __HAL_RCC_VREF_CLK_ENABLE();
  HAL_SYSCFG_VREFBUF_VoltageScalingConfig(SYSCFG_VREFBUF_VOLTAGE_SCALE0);
  HAL_SYSCFG_VREFBUF_HighImpedanceConfig(SYSCFG_VREFBUF_HIGH_IMPEDANCE_DISABLE);
  SET_BIT(VREFBUF->CSR, VREFBUF_CSR_ENVR);

  uint32_t start = millis();
  while ((VREFBUF->CSR & VREFBUF_CSR_VRR) == 0U) {
    if (millis() - start > 10U) {
      return false;
    }
  }
  return true;
}

openplc_reset_cause_t openplcResetCause(void)
{
  return openplc_reset_cause();
}
