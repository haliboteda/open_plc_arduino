/*
 * USB_Serial -- the USB port as a serial port.
 *
 * What it does: sends back every line you type in the Serial Monitor, and
 * counts them.
 * Connect: the board's USB port to the PC.
 * Expect: each line comes back with its number.
 * Serial Monitor: the board's USB port, 115200 baud. After every upload or
 * power cycle, close and reopen it (the IDE's Serial Monitor does this itself);
 * a terminal must have DTR on.
 */

OPENPLC_APP_VERSION(1, 0, 0);

String line;
int count = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial) {}  // waits for the PC: only for sketches that need one
  delay(100);         // a terminal clears its input just after opening
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
