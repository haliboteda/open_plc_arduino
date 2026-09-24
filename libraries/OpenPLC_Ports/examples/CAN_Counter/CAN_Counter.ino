/*
 * CAN_Counter -- the CAN port at 500 kbit/s.
 *
 * What it does: sends a frame with ID 0x123 once a second, carrying a counter
 * in its first four bytes, and prints every frame it receives.
 *
 * Wiring: a second CAN node (another board, or a USB-CAN adapter) on CAN H,
 * CAN L and CAN GND, set to 500 kbit/s. This board has no termination fitted,
 * so switch on the 120 ohm terminator of the other node. Without a second node
 * nothing acknowledges the frames and nothing is received.
 * Expect: the other node sees ID 0x123 once a second with the counter going up;
 * frames it sends appear here as "RX id=... len=... data=...".
 *
 * Serial Monitor: the board's USB port, 115200 baud.
 */

#include <OpenPLC_CAN.h>

OPENPLC_APP_VERSION(1, 0, 0);

uint32_t counter = 0;
uint32_t lastSend = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}
  if (!CAN.begin(500000)) {
    Serial.println("CAN_Counter: CAN did not start.");
    while (true) {}
  }
  Serial.println("CAN_Counter: sending ID 0x123 every second, printing what arrives.");
}

void loop() {
  if (millis() - lastSend >= 1000) {
    lastSend = millis();
    uint8_t data[4] = {
      (uint8_t)(counter >> 24), (uint8_t)(counter >> 16),
      (uint8_t)(counter >> 8), (uint8_t)counter
    };
    Serial.print("TX id=0x123 counter=");
    Serial.print(counter);
    Serial.println(CAN.write(0x123, data, sizeof(data)) ? "" : " (TX queue full)");
    counter++;
  }

  uint32_t id;
  uint8_t data[8];
  uint8_t len;
  while (CAN.read(id, data, len)) {
    Serial.print("RX id=0x");
    Serial.print(id, HEX);
    Serial.print(" len=");
    Serial.print(len);
    Serial.print(" data=");
    for (uint8_t i = 0; i < len; i++) {
      if (data[i] < 0x10) Serial.print('0');
      Serial.print(data[i], HEX);
      Serial.print(' ');
    }
    Serial.println();
  }
}
