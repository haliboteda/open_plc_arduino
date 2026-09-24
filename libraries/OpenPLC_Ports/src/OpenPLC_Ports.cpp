/*
 * OpenPLC_Ports.cpp -- see OpenPLC_Ports.h.
 */

#include "OpenPLC_Ports.h"

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
