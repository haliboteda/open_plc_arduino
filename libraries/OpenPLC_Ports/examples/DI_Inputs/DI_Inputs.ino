/*
 * DI_Inputs -- the eight digital inputs DI1..DI8.
 *
 * What it does: prints the state of DI1..DI8 whenever one of them changes.
 * Connect: 24 V supply to the board; switch 24 V onto the input you test.
 * Expect: that input reads 1 while 24 V is applied and 0 when it is removed.
 *   With no 24 V supply at all, every input reads 1 -- that is the board, not
 *   a fault in the input.
 * Serial Monitor: on the board's USB port.
 */

OPENPLC_APP_VERSION(1, 0, 0);

const int inputs[] = {DIN_1, DIN_2, DIN_3, DIN_4, DIN_5, DIN_6, DIN_7, DIN_8};
const int count = sizeof(inputs) / sizeof(inputs[0]);
int last = -1;

void printBanner() {
  Serial.println("DI_Inputs: prints DI1..DI8 on every change.");
  last = -1;  // and the current state right after it
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
    pinMode(inputs[i], INPUT);  // the board has its own pull-ups; do not add INPUT_PULLUP
  }
}

void loop() {
  printBannerOnConnect();
  int now = 0;
  for (int i = 0; i < count; i++) {
    now |= digitalRead(inputs[i]) << i;
  }
  if (now != last) {
    Serial.print("DI1..DI8: ");
    for (int i = 0; i < count; i++) {
      Serial.print((now >> i) & 1);
      Serial.print(i + 1 < count ? " " : "\n");
    }
    last = now;
  }
  delay(20);
}
