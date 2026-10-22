/* T3-25: AO turns the DAC buffer off at code 0 and on otherwise (decision
 * 152), against a DAC that ignores buffer writes while enabled. The channel
 * starts the way analogWrite() leaves it: enabled, buffer on. */
#include <stdio.h>

#include "stm32_def.h"
#include "openplc_ao_dac.h"

extern unsigned test_dac_stops[2];

static int failures;

static void expect(int ch, int buffer_off, uint32_t code, unsigned stops,
                   const char *step)
{
  uint32_t shift = (ch == 1) ? DAC_MCR_MODE1_Pos : DAC_MCR_MODE2_Pos;
  uint32_t en = DAC_CR_EN1 << ((ch == 1) ? DAC_CHANNEL_1 : DAC_CHANNEL_2);
  int off = ((test_dac1.MCR >> shift) & DAC_MCR_MODE1_1) != 0U;
  int on = (test_dac1.CR & en) != 0U;
  uint32_t got = test_dac1.DHR12R[ch - 1];
  unsigned s = test_dac_stops[ch - 1];

  if (off != buffer_off || !on || got != code || s != stops) {
    printf("FAIL %s: AO%d buffer %s (want %s), enabled %d, code %u (want %u), "
           "stops %u (want %u)\n", step, ch, off ? "off" : "on",
           buffer_off ? "off" : "on", on, (unsigned)got, (unsigned)code, s, stops);
    failures++;
  }
}

int main(void)
{
  /* What analogWrite() left behind: both channels enabled, buffer on. */
  test_dac1.CR = (DAC_CR_EN1 << DAC_CHANNEL_1) | (DAC_CR_EN1 << DAC_CHANNEL_2);

  openplc_ao_dac_write(1, 1000U); expect(1, 0, 1000U, 0U, "AO1 1000 first, no stop");
  openplc_ao_dac_write(1, 0U);    expect(1, 1, 0U,    1U, "AO1 to 0");
  openplc_ao_dac_write(1, 0U);    expect(1, 1, 0U,    1U, "AO1 0 again");
  openplc_ao_dac_write(1, 500U);  expect(1, 0, 500U,  2U, "AO1 back to 500");
  openplc_ao_dac_write(1, 800U);  expect(1, 0, 800U,  2U, "AO1 500 -> 800, no stop");
  openplc_ao_dac_write(1, 0U);    expect(1, 1, 0U,    3U, "AO1 to 0 again");

  /* AO2 on its own bits; AO1 untouched. */
  openplc_ao_dac_write(2, 0U);    expect(2, 1, 0U,    1U, "AO2 0 first");
  openplc_ao_dac_write(2, 1000U); expect(2, 0, 1000U, 2U, "AO2 to 1000");
  expect(1, 1, 0U, 3U, "AO1 after AO2 writes");

  if (failures) {
    printf("T3-25: %d failure(s)\n", failures);
    return 1;
  }
  printf("T3-25: PASS\n");
  return 0;
}
