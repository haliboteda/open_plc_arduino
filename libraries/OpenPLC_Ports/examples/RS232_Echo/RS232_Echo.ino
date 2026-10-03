/*
 * RS232_Echo -- the RS232 port.
 *
 * What it does: sends back every byte it receives on RS232, and reports it on
 * the Serial Monitor.
 * Connect: a PC through an RS232 adapter (real RS232 levels, not a 3.3 V
 *   USB-TTL adapter), 115200 8N1, in any terminal program.
 * Expect: what you type in that terminal comes straight back. The same port
 *   also carries the board's own lines: the bootloader's at power-up, and one
 *   "[NET] ip=..." line once the board has an address.
 * Serial Monitor: the board's USB port, 115200 baud.
 */

OPENPLC_APP_VERSION(1, 0, 0);

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}
  pinMode(RS232_EN_Pin, OUTPUT);
  digitalWrite(RS232_EN_Pin, HIGH);  // the transceiver is off until this pin is high
  Serial_Test.begin(115200);
  Serial.println("RS232_Echo: type in a terminal on the RS232 port.");
}

void loop() {
  while (Serial_Test.available()) {
    int c = Serial_Test.read();
    Serial_Test.write(c);
    Serial.print("RS232 got 0x");
    Serial.println(c, HEX);
  }
}
