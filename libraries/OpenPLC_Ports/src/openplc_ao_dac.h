/*
 * openplc_ao_dac.h -- writes an AO DAC code with the output buffer off at 0.
 * Design: $PROD/docs/modules/M3/CALIBRATED-ANALOG.md, decision 152.
 */

#ifndef OPENPLC_AO_DAC_H_
#define OPENPLC_AO_DAC_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* channel 1 = AO1 (DAC1_OUT1), 2 = AO2 (DAC1_OUT2). The DAC clock and pin
 * must already be up: openplcWriteAO_mA() does that once through analogWrite(). */
void openplc_ao_dac_write(uint8_t channel, uint32_t code);

#ifdef __cplusplus
}
#endif

#endif
