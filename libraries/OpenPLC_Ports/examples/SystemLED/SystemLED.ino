/*
 * SystemLED -- the system indicator LED.
 *
 * What it does: blinks the system LED once a second.
 * Connect: nothing.
 * Expect: the LED on and off for half a second each, and "on" / "off" on the
 *   Serial Monitor in step with it.
 * Serial Monitor: on the board's USB port.
 */

OPENPLC_APP_VERSION(1, 0, 0);

const int led = PE2;  // system LED, on when high

void printBanner() {
  Serial.println("SystemLED: blinks once a second.");
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
  pinMode(led, OUTPUT);
}

void loop() {
  printBannerOnConnect();
  digitalWrite(led, HIGH);
  Serial.println("on");
  delay(500);
  digitalWrite(led, LOW);
  Serial.println("off");
  delay(500);
}
