/*
 * AO_Outputs -- the two analog outputs AO1 and AO2, which drive a current.
 *
 * What it does: steps both outputs through 0, 5, 10, 15 and 20 mA, three
 * seconds each, and prints the current each step should produce.
 * Connect: a multimeter on the mA range in series with each output.
 * Expect: the meter reads the printed current. The EF lines are the two
 *   outputs' fault lines, printed as raw levels.
 * Serial Monitor: the board's USB port, 115200 baud.
 */

#include <OpenPLC_Ports.h>

OPENPLC_APP_VERSION(1, 0, 0);

const uint32_t steps_ua[] = {0, 5000, 10000, 15000, 20000};

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}
  if (!openplcEnableVref()) {
    Serial.println("AO_Outputs: the internal reference did not start; outputs are meaningless.");
  }
  analogWriteResolution(12);
  pinMode(AOUT_1_EF, INPUT);
  pinMode(AOUT_2_EF, INPUT);
  Serial.println("AO_Outputs: both outputs step through 0..20 mA.");
}

void loop() {
  for (uint32_t ua : steps_ua) {
    uint32_t pin_mv = ua * 1024 / 10000;  // XTR111: Iout = Vin * 10 / 1024 R
    uint32_t code = pin_mv * 4095 / OPENPLC_VREF_MV;
    analogWrite(AOUT_1, code);
    analogWrite(AOUT_2, code);
    delay(100);
    Serial.print("AO1 = AO2 = ");
    Serial.print(ua);
    Serial.print(" uA   EF1 = ");
    Serial.print(digitalRead(AOUT_1_EF));
    Serial.print("  EF2 = ");
    Serial.println(digitalRead(AOUT_2_EF));
    delay(2900);
  }
}
