#ifndef RS485SERIAL_H
#define RS485SERIAL_H

#include "HardwareSerial.h"

/* A HardwareSerial that drives the RS485 bus only while sending: every write
 * raises the direction pin, waits until the last byte has left (TC), and
 * drops it again, so a sketch never touches the pin. */
class RS485Serial : public HardwareSerial {
  public:
    RS485Serial(uint32_t rx, uint32_t tx, uint32_t dir);
    void begin(unsigned long baud)
    {
      begin(baud, SERIAL_8N1);
    }
    void begin(unsigned long baud, byte config);
    size_t write(uint8_t c) override;
    size_t write(const uint8_t *buffer, size_t size) override;
    using HardwareSerial::write;

  private:
    uint32_t _dir;
    bool _sending;
};

#endif
