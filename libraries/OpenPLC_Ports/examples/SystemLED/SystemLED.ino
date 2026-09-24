/*
 * SystemLED -- the system indicator LED.
 *
 * What it does: blinks the system LED once a second.
 * Connect: nothing.
 * Expect: the LED on and off for half a second each, and "on" / "off" on the
 *   Serial Monitor in step with it.
 * Serial Monitor: the board's USB port, 115200 baud.
 */

OPENPLC_APP_VERSION(1, 0, 0);

const int led = PE2;  // system LED, on when high

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}
  pinMode(led, OUTPUT);
  Serial.println("SystemLED: blinks once a second.");
}

void loop() {
  digitalWrite(led, HIGH);
  Serial.println("on");
  delay(500);
  digitalWrite(led, LOW);
  Serial.println("off");
  delay(500);
}
