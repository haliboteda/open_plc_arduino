/*
 * BoardTemperature -- the two temperature sensors on the board.
 *
 * What it does: prints both temperatures once a second -- one sits at the
 * short-circuit protection, the other at the digital-output switches.
 * Connect: nothing.
 * Expect: both close to room temperature, rising when the outputs work hard.
 * Serial Monitor: on the board's USB port.
 */

#include <OpenPLC_Ports.h>

OPENPLC_APP_VERSION(1, 0, 0);

bool vrefOk = false;

void printBanner() {
  if (!vrefOk) {
    Serial.println("BoardTemperature: the internal reference did not start; readings are meaningless.");
  }
  Serial.println("BoardTemperature: degrees C, once a second.");
}

// Prints the description each time the Serial Monitor connects, not once in
// setup(): every reset re-enumerates USB and drops what setup() printed (README).
void printBannerOnConnect() {
  static bool wasConnected = false;
  static uint32_t lastCheck = 0;
  if (millis() - lastCheck < 250) {
    return;  // (bool)Serial takes 10 ms on USB
  }
  lastCheck = millis();
  bool now = (bool)Serial;
  if (now && !wasConnected) {
    printBanner();
  }
  wasConnected = now;
}

void setup() {
  Serial.begin(115200);
  vrefOk = openplcEnableVref();
  analogReadResolution(12);
}

float celsius(int pin) {
  float mv = analogRead(pin) * (float)OPENPLC_VREF_MV / 4095.0f;
  return (mv - 500.0f) / 10.0f;  // 500 mV at 0 C, 10 mV per degree
}

void loop() {
  printBannerOnConnect();
  Serial.print("protection = ");
  Serial.print(celsius(TEMP_SCPROT), 1);
  Serial.print(" C   output switches = ");
  Serial.print(celsius(TEMP_HSSW), 1);
  Serial.println(" C");
  delay(1000);
}
