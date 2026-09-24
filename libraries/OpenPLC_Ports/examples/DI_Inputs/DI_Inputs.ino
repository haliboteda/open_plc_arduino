/*
 * DI_Inputs -- the eight digital inputs DI1..DI8.
 *
 * What it does: prints the state of DI1..DI8 whenever one of them changes.
 * Connect: 24 V supply to the board; switch 24 V onto the input you test.
 * Expect: that input reads 1 while 24 V is applied and 0 when it is removed.
 *   With no 24 V supply at all, every input reads 1 -- that is the board, not
 *   a fault in the input.
 * Serial Monitor: the board's USB port, 115200 baud.
 */

OPENPLC_APP_VERSION(1, 0, 0);

const int inputs[] = {DIN_1, DIN_2, DIN_3, DIN_4, DIN_5, DIN_6, DIN_7, DIN_8};
const int count = sizeof(inputs) / sizeof(inputs[0]);
int last = -1;

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}
  for (int i = 0; i < count; i++) {
    pinMode(inputs[i], INPUT);  // the board has its own pull-ups; do not add INPUT_PULLUP
  }
  Serial.println("DI_Inputs: prints DI1..DI8 on every change.");
}

void loop() {
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
