/* Host stand-in for the core's stm32_def.h: only the DAC HAL that
 * openplc_ao_dac.c calls. The DAC behind it is fake_dac.c. */
#ifndef STM32_DEF_H_STUB
#define STM32_DEF_H_STUB

#include <stdint.h>

typedef enum { HAL_OK = 0, HAL_ERROR = 1 } HAL_StatusTypeDef;

typedef struct {
  uint32_t CR;
  uint32_t MCR;
  uint32_t DHR12R[2];
} DAC_TypeDef;

extern DAC_TypeDef test_dac1;
#define DAC1 (&test_dac1)

typedef struct {
  DAC_TypeDef *Instance;
} DAC_HandleTypeDef;

typedef struct {
  uint32_t DAC_SampleAndHold;
  uint32_t DAC_Trigger;
  uint32_t DAC_OutputBuffer;
  uint32_t DAC_ConnectOnChipPeripheral;
  uint32_t DAC_UserTrimming;
} DAC_ChannelConfTypeDef;

#define DAC_CHANNEL_1               0x00U
#define DAC_CHANNEL_2               0x10U
#define DAC_CR_EN1                  (1U << 0)
#define DAC_MCR_MODE1_Pos           0U
#define DAC_MCR_MODE2_Pos           16U
#define DAC_MCR_MODE1               (7U << 0)
#define DAC_MCR_MODE1_1             (1U << 1)
#define DAC_OUTPUTBUFFER_ENABLE     0U
#define DAC_OUTPUTBUFFER_DISABLE    DAC_MCR_MODE1_1
#define DAC_SAMPLEANDHOLD_DISABLE   0U
#define DAC_TRIGGER_NONE            0U
#define DAC_CHIPCONNECT_EXTERNAL    0U
#define DAC_TRIMMING_FACTORY        0U
#define DAC_ALIGN_12B_R             0U

HAL_StatusTypeDef HAL_DAC_Init(DAC_HandleTypeDef *h);
HAL_StatusTypeDef HAL_DAC_ConfigChannel(DAC_HandleTypeDef *h,
                                        const DAC_ChannelConfTypeDef *c,
                                        uint32_t ch);
HAL_StatusTypeDef HAL_DAC_SetValue(DAC_HandleTypeDef *h, uint32_t ch,
                                   uint32_t align, uint32_t v);
HAL_StatusTypeDef HAL_DAC_Start(DAC_HandleTypeDef *h, uint32_t ch);
HAL_StatusTypeDef HAL_DAC_Stop(DAC_HandleTypeDef *h, uint32_t ch);

#endif
