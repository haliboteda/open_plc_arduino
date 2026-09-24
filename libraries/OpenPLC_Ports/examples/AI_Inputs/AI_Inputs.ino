/*
 * AI_Inputs -- the two analog inputs: AI1 measures voltage, AI2 current.
 *
 * What it does: prints AI1 in millivolts and AI2 in microamps once a second.
 * Connect: a 0..10 V source on AI1, a 0..20 mA source on AI2.
 *   Both need their solder jumpers closed on the board, or they read about 0.
 * Expect: the printed value follows the source (full scale 10 V and 20 mA).
 * Serial Monitor: the board's USB port, 115200 baud.
 */

#include <OpenPLC_Ports.h>

OPENPLC_APP_VERSION(1, 0, 0);

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}
  if (!openplcEnableVref()) {
    Serial.println("AI_Inputs: the internal reference did not start; readings are meaningless.");
  }
  analogReadResolution(12);
  Serial.println("AI_Inputs: AI1 in mV, AI2 in uA, once a second.");
}

void loop() {
  uint32_t pin1 = analogRead(AIN_1) * OPENPLC_VREF_MV / 4095;
  uint32_t pin2 = analogRead(AIN_2) * OPENPLC_VREF_MV / 4095;
  uint32_t ai1_mv = pin1 * 40089 / 10000;  // 22.6k / 90.6k divider
  uint32_t ai2_ua = pin2 * 1000 / 124;     // 124 R shunt

  Serial.print("AI1 = ");
  Serial.print(ai1_mv);
  Serial.print(" mV   AI2 = ");
  Serial.print(ai2_ua);
  Serial.println(" uA");
  delay(1000);
}
