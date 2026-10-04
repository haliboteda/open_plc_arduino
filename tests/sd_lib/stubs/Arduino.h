/* Just enough of the Arduino core for OpenPLC_SD.cpp on the host. */
#ifndef HOST_ARDUINO_H_
#define HOST_ARDUINO_H_

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define LOW  0
#define HIGH 1

int digitalRead(uint32_t pin);

class Print {
  public:
    virtual ~Print() {}
    virtual size_t write(uint8_t b) = 0;
    virtual size_t write(const uint8_t *buf, size_t len)
    {
        size_t n = 0;
        while (len--) {
            n += write(*buf++);
        }
        return n;
    }
    size_t write(const char *s) { return write((const uint8_t *)s, strlen(s)); }
    size_t print(const char *s) { return write(s); }
    virtual void flush() {}
    void setWriteError(int e = 1) { err_ = e; }
    int getWriteError() const { return err_; }

  private:
    int err_ = 0;
};

class Stream : public Print {
  public:
    virtual int available() = 0;
    virtual int read() = 0;
    virtual int peek() = 0;
};

#endif
