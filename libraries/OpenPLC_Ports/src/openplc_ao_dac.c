#include "openplc_ao_dac.h"
#include "stm32_def.h"

/* Buffer off at code 0, on otherwise: buffered, 0 leaves ~0.16 mA at the
 * terminal; unbuffered cannot drive R17/R18 above 0. See $PROD
 * docs/tables/DECISIONS.md 152.
 * MODEx is writable only while ENx = 0 (RM0433 DAC_MCR), so a buffer change
 * always goes stop -> configure -> value -> start. */
void openplc_ao_dac_write(uint8_t channel, uint32_t code)
{
  DAC_HandleTypeDef h = {0};
  uint32_t ch = (channel == 1U) ? DAC_CHANNEL_1 : DAC_CHANNEL_2;
  uint32_t shift = (channel == 1U) ? DAC_MCR_MODE1_Pos : DAC_MCR_MODE2_Pos;
  int off = (code == 0U);
  int is_off = ((DAC1->MCR >> shift) & DAC_MCR_MODE1_1) != 0U;

  h.Instance = DAC1;
  if (off != is_off) {
    DAC_ChannelConfTypeDef cfg = {0};
    cfg.DAC_SampleAndHold           = DAC_SAMPLEANDHOLD_DISABLE;
    cfg.DAC_Trigger                 = DAC_TRIGGER_NONE;
    cfg.DAC_OutputBuffer            = off ? DAC_OUTPUTBUFFER_DISABLE : DAC_OUTPUTBUFFER_ENABLE;
    cfg.DAC_ConnectOnChipPeripheral = DAC_CHIPCONNECT_EXTERNAL;
    cfg.DAC_UserTrimming            = DAC_TRIMMING_FACTORY;

    (void)HAL_DAC_Stop(&h, ch);
    if (HAL_DAC_ConfigChannel(&h, &cfg, ch) != HAL_OK) {
      return;
    }
  }
  (void)HAL_DAC_SetValue(&h, ch, DAC_ALIGN_12B_R, code);
  (void)HAL_DAC_Start(&h, ch);
}
