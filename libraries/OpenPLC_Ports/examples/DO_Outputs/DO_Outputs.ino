/*
 * DO_Outputs -- the eight digital outputs DO1..DO8.
 *
 * What it does: switches DO1..DO8 on one at a time, one second each, and says
 * on the Serial Monitor which one is on.
 * Connect: a load or a multimeter on the output you want to watch.
 * Expect: that output is on exactly while the Serial Monitor says so.
 * Serial Monitor: on the board's USB port.
 */

OPENPLC_APP_VERSION(1, 0, 0);

const int outputs[] = {DOUT_1, DOUT_2, DOUT_3, DOUT_4, DOUT_5, DOUT_6, DOUT_7, DOUT_8};
const int count = sizeof(outputs) / sizeof(outputs[0]);

void printBanner() {
  Serial.println("DO_Outputs: each output turns on for 1 s, in order.");
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
  for (int i = 0; i < count; i++) {
    pinMode(outputs[i], OUTPUT);
    digitalWrite(outputs[i], LOW);
  }
}

void loop() {
  for (int i = 0; i < count; i++) {
    printBannerOnConnect();
    Serial.print("DO");
    Serial.print(i + 1);
    Serial.println(" on");
    digitalWrite(outputs[i], HIGH);
    delay(1000);
    digitalWrite(outputs[i], LOW);
  }
}
