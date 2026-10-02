/*
 * AI_Inputs -- the two analog inputs: AI1 measures voltage, AI2 current.
 *
 * What it does: prints AI1 in millivolts and AI2 in milliamps once a second,
 *   corrected with this board's calibration from the production fixture.
 * Connect: a 0..10 V source on AI1, a 0..20 mA source on AI2.
 *   Both need their solder jumpers closed on the board, or they read about 0.
 * Expect: first a line saying whether the board is calibrated, then the
 *   printed values follow the source (full scale 10000 mV and 20 mA). An
 *   uncalibrated board still reads, with the nominal conversion.
 * Serial Monitor: the board's USB port, 115200 baud.
 */

#include <OpenPLC_Ports.h>

OPENPLC_APP_VERSION(1, 1, 0);

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}
  if (openplcCalibrationStatus() == CALIB_OK) {
    Serial.println("AI_Inputs: this board is calibrated.");
  } else {
    Serial.println("AI_Inputs: no valid calibration, nominal conversion.");
  }
  Serial.println("AI_Inputs: AI1 in mV, AI2 in mA, once a second.");
}

void loop() {
  Serial.print("AI1 = ");
  Serial.print(openplcReadAI1_mV(), 0);
  Serial.print(" mV   AI2 = ");
  Serial.print(openplcReadAI2_mA(), 3);
  Serial.println(" mA");
  delay(1000);
}
