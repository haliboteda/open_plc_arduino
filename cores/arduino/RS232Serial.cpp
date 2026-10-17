#include "Arduino.h"
#include "RS232Serial.h"

RS232Serial::RS232Serial(PinName rx, PinName tx, uint32_t en)
  : HardwareSerial(rx, tx), _en(en)
{
}

void RS232Serial::begin(unsigned long baud, byte config)
{
  // Pins first: with the transceiver on, an unconfigured TX pin would drive
  // the line. Enable pin facts: $PROD/docs/hardware/HARDWARE-FACTS.md "RS232 路径".
  HardwareSerial::begin(baud, config);
  pinMode(_en, OUTPUT);
  digitalWrite(_en, HIGH);
}

void RS232Serial::end()
{
  HardwareSerial::end();  // flushes pending output first
  digitalWrite(_en, LOW);
}
