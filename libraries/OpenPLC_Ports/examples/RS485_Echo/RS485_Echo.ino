/*
 * RS485_Echo -- the RS485 port.
 *
 * What it does: sends back every line it receives on RS485, and reports it on
 * the Serial Monitor.
 * Connect: a PC through a USB-RS485 adapter to terminals C10 (A) / C11 (B),
 *   A to A and B to B, 115200 8N1.
 * Expect: each line you send comes back.
 * Serial Monitor: on the board's USB port.
 */

OPENPLC_APP_VERSION(1, 0, 0);

String line;

void printBanner() {
  Serial.println("RS485_Echo: send a line over RS485.");
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
  RS485.begin(115200);  // drives the bus only while sending
}

void loop() {
  printBannerOnConnect();
  while (RS485.available()) {
    char c = RS485.read();
    line += c;
    if (c == '\n') {
      RS485.print(line);
      Serial.print("RS485 echoed: ");
      Serial.print(line);
      line = "";
    }
  }
}
