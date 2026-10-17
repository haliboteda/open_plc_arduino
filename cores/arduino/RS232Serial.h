#ifndef RS232SERIAL_H
#define RS232SERIAL_H

#include "HardwareSerial.h"

/* A HardwareSerial that powers the RS232 transceiver only while it is open:
 * begin() sets up the UART pins and then raises the enable pin, end() lowers
 * it. The core never opens it, so the terminals belong to the sketch
 * (R3-23, $PROD/docs/modules/M3-app-runtime.md). */
class RS232Serial : public HardwareSerial {
  public:
    RS232Serial(PinName rx, PinName tx, uint32_t en);
    void begin(unsigned long baud)
    {
      begin(baud, SERIAL_8N1);
    }
    void begin(unsigned long baud, byte config);
    void end();

  private:
    uint32_t _en;
};

#endif
