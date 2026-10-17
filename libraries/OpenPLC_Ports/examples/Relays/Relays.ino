/*
 * Relays -- the six relays RY1..RY6.
 *
 * What it does: at start-up closes RY1..RY6 one at a time, 1000 ms each, for
 * one round, then stops with every relay open. Commands typed in the Serial
 * Monitor (line ending "Newline"):
 *   r 3        3 rounds from the next run (1..10000, default 1)
 *   i 500      each relay closed for 500 ms from the next run
 *              (10..60000, default 1000)
 *   g          run again
 *   any line typed during a run stops it, every relay open.
 * Over 100 rounds or under 1000 ms is still set, with a "warning:" line:
 * relay contacts wear with every switching. The relays are Hongfa
 * HF41F/005-HST; its datasheet rates contact life at 1 s on, 9 s off.
 * Connect: nothing needed -- you hear each relay click. To check the contact,
 *   put a multimeter (continuity) across that relay's terminals.
 * Expect: one click on and one click off per relay, in order, then quiet;
 *   "done, all relays open" at the end of a run.
 * Serial Monitor: on the board's USB port.
 */

OPENPLC_APP_VERSION(1, 0, 0);

const int relays[] = {REL_1, REL_2, REL_3, REL_4, REL_5, REL_6};
const int count = sizeof(relays) / sizeof(relays[0]);
const long WARN_ROUNDS = 100;
const long WARN_MS = 1000;
const long GAP_MS = 200;

String line;
long rounds = 1;
long closedMs = 1000;
const char *startupEnd = nullptr;  // last line of the start-up round, until g

void printBanner() {
  Serial.println("Relays: closes RY1..RY6 one at a time, then stops; r <rounds>, i <ms> and g to run again.");
  if (startupEnd != nullptr) {
    Serial.println(startupEnd);
  }
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

// False if something arrived on the Serial Monitor before ms ran out.
bool wait(long ms) {
  unsigned long start = millis();
  while ((long)(millis() - start) < ms) {
    printBannerOnConnect();
    if (Serial.available()) {
      return false;
    }
  }
  return true;
}

// Returns the line it ended with.
const char *run() {
  Serial.print("run: ");
  Serial.print(rounds);
  Serial.print(" round(s), ");
  Serial.print(closedMs);
  Serial.println(" ms per relay");
  for (long r = 0; r < rounds; r++) {
    for (int i = 0; i < count; i++) {
      Serial.print("RY");
      Serial.print(i + 1);
      Serial.println(" closed");
      digitalWrite(relays[i], HIGH);
      bool ok = wait(closedMs);
      digitalWrite(relays[i], LOW);
      if (!ok || !wait(GAP_MS)) {
        delay(20);  // let the rest of the typed line arrive, then drop it
        while (Serial.available()) {
          Serial.read();
        }
        Serial.println("stopped, all relays open");
        return "stopped, all relays open";
      }
    }
  }
  Serial.println("done, all relays open");
  return "done, all relays open";
}

void command(String s) {
  s.trim();
  if (s == "g") {
    startupEnd = nullptr;
    run();
    return;
  }
  int space = s.indexOf(' ');
  String head = space < 0 ? s : s.substring(0, space);
  long value = space < 0 ? 0 : s.substring(space + 1).toInt();
  if (head == "r" && value >= 1 && value <= 10000) {
    rounds = value;
    Serial.print("rounds = ");
    Serial.println(rounds);
    if (rounds > WARN_ROUNDS) {
      Serial.println("warning: over 100 rounds -- every round switches each relay once and the contacts wear");
    }
    return;
  }
  if (head == "i" && value >= 10 && value <= 60000) {
    closedMs = value;
    Serial.print("interval = ");
    Serial.print(closedMs);
    Serial.println(" ms");
    if (closedMs < WARN_MS) {
      Serial.println("warning: under 1000 ms -- the relay life is rated at 1 s on, 9 s off");
    }
    return;
  }
  Serial.println("? use: r <rounds 1-10000>, i <ms 10-60000>, or g");
}

void setup() {
  Serial.begin(115200);
  for (int i = 0; i < count; i++) {
    pinMode(relays[i], OUTPUT);
    digitalWrite(relays[i], LOW);
  }
  startupEnd = run();
}

void loop() {
  printBannerOnConnect();
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (line.length() > 0) {
        command(line);
        line = "";
      }
    } else {
      line += c;
    }
  }
}
