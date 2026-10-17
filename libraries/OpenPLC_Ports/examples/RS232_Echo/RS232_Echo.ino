/*
 * RS232_Echo -- the RS232 port.
 *
 * What it does: sends back every byte it receives on RS232, and after each
 * line reports on the Serial Monitor how many bytes that line had.
 * Connect: a PC to terminals C05 / C06 through an RS232 adapter (real RS232
 *   levels, not a 3.3 V USB-TTL adapter), 115200 8N1, in any terminal program.
 * Expect: what you type in that terminal comes straight back; after each
 *   Enter the Serial Monitor shows "RS232_Echo: got N bytes".
 * Serial Monitor: on the board's USB port.
 */

OPENPLC_APP_VERSION(1, 1, 0);

uint32_t count = 0;

void printBanner() {
  Serial.println("RS232_Echo: type in a terminal on the RS232 port.");
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
  RS232.begin(115200);  // also switches the RS232 transceiver on
}

void loop() {
  printBannerOnConnect();
  while (RS232.available()) {
    int c = RS232.read();
    RS232.write(c);
    count++;
    if (c == '\n') {
      Serial.print("RS232_Echo: got ");
      Serial.print(count);
      Serial.println(" bytes");
      count = 0;
    }
  }
}
