#include "Arduino.h"
#include "RS485Serial.h"

RS485Serial::RS485Serial(uint32_t rx, uint32_t tx, uint32_t dir)
  : HardwareSerial(rx, tx), _dir(dir), _sending(false)
{
}

void RS485Serial::begin(unsigned long baud, byte config)
{
  pinMode(_dir, OUTPUT);
  digitalWrite(_dir, LOW);
  HardwareSerial::begin(baud, config);
}

size_t RS485Serial::write(uint8_t c)
{
  return write(&c, 1);
}

size_t RS485Serial::write(const uint8_t *buffer, size_t size)
{
  // HardwareSerial::write() calls write() again when the data wraps the ring
  // buffer; only the outermost call may touch the pin.
  if (_sending) {
    return HardwareSerial::write(buffer, size);
  }
  _sending = true;
  digitalWrite(_dir, HIGH);
  size_t n = HardwareSerial::write(buffer, size);
  flush();
  digitalWrite(_dir, LOW);
  _sending = false;
  return n;
}
