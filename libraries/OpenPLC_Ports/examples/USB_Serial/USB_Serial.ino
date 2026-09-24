/*
 * USB_Serial -- the USB port as a serial port.
 *
 * What it does: sends back every line you type in the Serial Monitor, and
 * counts them.
 * Connect: the board's USB port to the PC.
 * Expect: each line comes back with its number.
 * Serial Monitor: the board's USB port, 115200 baud.
 */

OPENPLC_APP_VERSION(1, 0, 0);

String line;
int count = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}
  Serial.println("USB_Serial: type a line and press Enter.");
}

void loop() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (line.length() > 0) {
        Serial.print(++count);
        Serial.print(": ");
        Serial.println(line);
        line = "";
      }
    } else {
      line += c;
    }
  }
}
