/*
 * RS485_Echo -- the RS485 port.
 *
 * What it does: sends back every line it receives on RS485, and reports it on
 * the Serial Monitor.
 * Connect: a PC through a USB-RS485 adapter, A to A and B to B, 115200 8N1.
 * Expect: each line you send comes back.
 * Serial Monitor: the board's USB port, 115200 baud.
 */

OPENPLC_APP_VERSION(1, 0, 0);

HardwareSerial RS485(RS485_RX_Pin, RS485_TX_Pin);
String line;

void send(const String &s) {
  digitalWrite(RS485_DIR_Pin, HIGH);  // drive the bus only while sending
  RS485.print(s);
  RS485.flush();                      // wait for the last byte to leave
  digitalWrite(RS485_DIR_Pin, LOW);
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}
  pinMode(RS485_DIR_Pin, OUTPUT);
  digitalWrite(RS485_DIR_Pin, LOW);
  RS485.begin(115200);
  Serial.println("RS485_Echo: send a line over RS485.");
}

void loop() {
  while (RS485.available()) {
    char c = RS485.read();
    line += c;
    if (c == '\n') {
      send(line);
      Serial.print("RS485 echoed: ");
      Serial.print(line);
      line = "";
    }
  }
}
