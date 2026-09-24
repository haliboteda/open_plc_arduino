/*
 * DO_Outputs -- the eight digital outputs DO1..DO8.
 *
 * What it does: switches DO1..DO8 on one at a time, one second each, and says
 * on the Serial Monitor which one is on.
 * Connect: a load or a multimeter on the output you want to watch.
 * Expect: that output is on exactly while the Serial Monitor says so.
 * Serial Monitor: the board's USB port, 115200 baud.
 */

OPENPLC_APP_VERSION(1, 0, 0);

const int outputs[] = {DOUT_1, DOUT_2, DOUT_3, DOUT_4, DOUT_5, DOUT_6, DOUT_7, DOUT_8};
const int count = sizeof(outputs) / sizeof(outputs[0]);

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}
  for (int i = 0; i < count; i++) {
    pinMode(outputs[i], OUTPUT);
    digitalWrite(outputs[i], LOW);
  }
  Serial.println("DO_Outputs: each output turns on for 1 s, in order.");
}

void loop() {
  for (int i = 0; i < count; i++) {
    Serial.print("DO");
    Serial.print(i + 1);
    Serial.println(" on");
    digitalWrite(outputs[i], HIGH);
    delay(1000);
    digitalWrite(outputs[i], LOW);
  }
}
