/*
 * AO_Outputs -- the two analog outputs AO1 and AO2, which drive a current.
 *
 * What it does: steps both outputs through 0, 5, 10, 15 and 20 mA, three
 *   seconds each, corrected with this board's calibration from the production
 *   fixture, and prints the current each step should produce.
 * Connect: a multimeter on the mA range in series with each output.
 * Expect: first a line saying whether the board is calibrated, then the meter
 *   reads the printed current. The EF lines are the two outputs' fault lines,
 *   printed as raw levels.
 * Serial Monitor: the board's USB port, 115200 baud.
 */

#include <OpenPLC_Ports.h>

OPENPLC_APP_VERSION(1, 1, 0);

const float steps_ma[] = {0.0f, 5.0f, 10.0f, 15.0f, 20.0f};

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}
  pinMode(AOUT_1_EF, INPUT);
  pinMode(AOUT_2_EF, INPUT);
  if (openplcCalibrationStatus() == CALIB_OK) {
    Serial.println("AO_Outputs: this board is calibrated.");
  } else {
    Serial.println("AO_Outputs: no valid calibration, nominal conversion.");
  }
  Serial.println("AO_Outputs: both outputs step through 0..20 mA.");
}

void loop() {
  for (float ma : steps_ma) {
    openplcWriteAO_mA(1, ma);
    openplcWriteAO_mA(2, ma);
    delay(100);
    Serial.print("AO1 = AO2 = ");
    Serial.print(ma, 1);
    Serial.print(" mA   EF1 = ");
    Serial.print(digitalRead(AOUT_1_EF));
    Serial.print("  EF2 = ");
    Serial.println(digitalRead(AOUT_2_EF));
    delay(2900);
  }
}
