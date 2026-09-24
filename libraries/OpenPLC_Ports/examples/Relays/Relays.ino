/*
 * Relays -- the six relays RY1..RY6.
 *
 * What it does: closes RY1..RY6 one at a time, one second each, and says on
 * the Serial Monitor which one is closed.
 * Connect: nothing needed -- you hear each relay click. To check the contact,
 *   put a multimeter (continuity) across that relay's terminals.
 * Expect: one click on and one click off per relay, in order.
 * Serial Monitor: the board's USB port, 115200 baud.
 */

OPENPLC_APP_VERSION(1, 0, 0);

const int relays[] = {REL_1, REL_2, REL_3, REL_4, REL_5, REL_6};
const int count = sizeof(relays) / sizeof(relays[0]);

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}
  for (int i = 0; i < count; i++) {
    pinMode(relays[i], OUTPUT);
    digitalWrite(relays[i], LOW);
  }
  Serial.println("Relays: each relay closes for 1 s, in order.");
}

void loop() {
  for (int i = 0; i < count; i++) {
    Serial.print("RY");
    Serial.print(i + 1);
    Serial.println(" closed");
    digitalWrite(relays[i], HIGH);
    delay(1000);
    digitalWrite(relays[i], LOW);
    delay(200);
  }
}
