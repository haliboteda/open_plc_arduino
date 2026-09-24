/*
 * BoardTemperature -- the two temperature sensors on the board.
 *
 * What it does: prints both temperatures once a second -- one sits at the
 * short-circuit protection, the other at the digital-output switches.
 * Connect: nothing.
 * Expect: both close to room temperature, rising when the outputs work hard.
 * Serial Monitor: the board's USB port, 115200 baud.
 */

#include <OpenPLC_Ports.h>

OPENPLC_APP_VERSION(1, 0, 0);

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}
  if (!openplcEnableVref()) {
    Serial.println("BoardTemperature: the internal reference did not start; readings are meaningless.");
  }
  analogReadResolution(12);
  Serial.println("BoardTemperature: degrees C, once a second.");
}

float celsius(int pin) {
  float mv = analogRead(pin) * (float)OPENPLC_VREF_MV / 4095.0f;
  return (mv - 500.0f) / 10.0f;  // 500 mV at 0 C, 10 mV per degree
}

void loop() {
  Serial.print("protection = ");
  Serial.print(celsius(TEMP_SCPROT), 1);
  Serial.print(" C   output switches = ");
  Serial.print(celsius(TEMP_HSSW), 1);
  Serial.println(" C");
  delay(1000);
}
