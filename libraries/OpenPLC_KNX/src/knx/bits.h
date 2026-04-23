#pragma  once

#include <cstddef>
#include <cstdint>

#if defined(__linux__)
    #include <arpa/inet.h>
#elif defined(ARDUINO_ARCH_STM32)
    /* On Arduino STM32, pull htons/htonl directly from LwIP so that ALL
     * compilation units (platform code AND KNX library files such as
     * ip_parameter_object.cpp) use the SAME byte-order functions.
     * If the bits.h custom byte-swap were used instead, htonl() in the
     * library files would differ from lwip_htonl() in platform code,
     * causing inverted IP addresses in KNXnet/IP discovery packets. */
    #include <lwip/def.h>      /* defines htons/htonl as lwip_htons/lwip_htonl */
    /* lwip/def.h maps htons→lwip_htons and htonl→lwip_htonl via macros,
     * so the #ifndef guards below are satisfied and no redefinition occurs. */
    #ifndef htons
    #  define htons(x) lwip_htons(x)
    #  define ntohs(x) lwip_ntohs(x)
    #endif
    #ifndef htonl
    #  define htonl(x) lwip_htonl(x)
    #  define ntohl(x) lwip_ntohl(x)
    #endif
#elif defined(ARDUINO_ARCH_SAMD) || defined(DeviceFamily_CC13X0)
    /* Use inline functions so htonl/htons accept rvalue arguments
     * (the getbyte macro takes &x, which requires an lvalue — unusable
     * with function-return temporaries like currentIpAddress()).
     * Guard against redefinition: LwIP maps htons → lwip_htons etc. */
    #ifndef htons
    static inline uint16_t _knx_htons(uint16_t x) {
        return (uint16_t)(((x & 0xFFu) << 8u) | ((x >> 8u) & 0xFFu));
    }
    #  define htons(x)  _knx_htons((uint16_t)(x))
    #  define ntohs(x)  _knx_htons((uint16_t)(x))
    #endif
    #ifndef htonl
    static inline uint32_t _knx_htonl(uint32_t x) {
        return ((x & 0xFFu) << 24u) | (((x >> 8u) & 0xFFu) << 16u) |
               (((x >> 16u) & 0xFFu) << 8u) | ((x >> 24u) & 0xFFu);
    }
    #  define htonl(x)  _knx_htonl((uint32_t)(x))
    #  define ntohl(x)  _knx_htonl((uint32_t)(x))
    #endif
#elif defined(LIBRETINY)
    #include <lwip/udp.h>
    #define htons(x) lwip_htons(x)
    #define htonl(x) lwip_htonl(x)
#endif

#if defined(ARDUINO_ARCH_SAMD) || defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_ARCH_STM32) || defined(LIBRETINY)
    #include <Arduino.h>
#elif defined(ARDUINO_ARCH_ESP8266)
    #include <Arduino.h>
    #include <user_interface.h>
#elif defined(ARDUINO_ARCH_ESP32)
    #include <Arduino.h>
    #include <esp_wifi.h>
#elif defined(ESP_PLATFORM)
    #include <lwip/inet.h>
    #include <driver/gpio.h>
    // // Define Arduino-like macros if needed for compatibility

    #define lowByte(val) ((val)&255)
    #define highByte(val) (((val) >> ((sizeof(val) - 1) << 3)) & 255)
    #define bitRead(val, bitno) (((val) >> (bitno)) & 1)
    #define DEC 10
    #define HEX 16
    #define OCT  8
    #define BIN  2
    #define LOW  0
    #define HIGH 1
    #define CHANGE GPIO_INTR_ANYEDGE
    #define FALLING GPIO_INTR_NEGEDGE
    #define RISING GPIO_INTR_POSEDGE
    // Implement or map Arduino-like functions if needed
    uint32_t millis();
    typedef void (*IsrFuncPtr)(void); // Arduino-style
    typedef void (*EspIsrFuncPtr)(void*); // ESP-IDF-style
    void attachInterrupt(uint32_t pin, IsrFuncPtr callback, uint32_t mode);
#else // Non-Arduino platforms
    #define lowByte(val) ((val)&255)
    #define highByte(val) (((val) >> ((sizeof(val) - 1) << 3)) & 255)
    #define bitRead(val, bitno) (((val) >> (bitno)) & 1)

    // print functions are implemented in the platform files
    #define DEC 10
    #define HEX 16

    #define INPUT (0x0)
    #define OUTPUT (0x1)
    #define INPUT_PULLUP (0x2)
    #define INPUT_PULLDOWN (0x3)

    #define LOW (0x0)
    #define HIGH (0x1)
    #define CHANGE 2
    #define FALLING 3
    #define RISING 4

    void delay(uint32_t millis);
    void delayMicroseconds (unsigned int howLong);
    uint32_t millis();
    void pinMode(uint32_t dwPin, uint32_t dwMode);
    void digitalWrite(uint32_t dwPin, uint32_t dwVal);
    uint32_t digitalRead(uint32_t dwPin);
    typedef void (*voidFuncPtr)(void);
    void attachInterrupt(uint32_t pin, voidFuncPtr callback, uint32_t mode);
#endif

#ifndef MIN
    #define MIN(a, b) ((a < b) ? (a) : (b))
#endif

#ifndef MAX
    #define MAX(a, b) ((a > b) ? (a) : (b))
#endif

#ifndef ABS
    #define ABS(x)    ((x > 0) ? (x) : (-x))
#endif

/* On Arduino STM32, delegate print()/println() free functions to a
 * HardwareSerial object.  Override KNX_DEBUG_SERIAL before including
 * this header if you want output on a different port. */
#if defined(ARDUINO_ARCH_STM32) && !defined(KNX_DEBUG_SERIAL)
#  define KNX_DEBUG_SERIAL Serial_Test
#  ifndef KNX_NO_PRINT
     /* Arduino.h (included above for ARDUINO_ARCH_STM32) provides HardwareSerial.
      * Forward-declare the instance so bits.cpp can forward print() calls to it. */
     extern HardwareSerial Serial_Test;
#  endif
#endif

#ifndef KNX_NO_PRINT
    void print(const char[]);
    void print(char);
    void print(unsigned char, int = DEC);
    void print(int, int = DEC);
    void print(unsigned int, int = DEC);
    void print(long, int = DEC);
    void print(unsigned long, int = DEC);
    void print(long long, int = DEC);
    void print(unsigned long long, int = DEC);
    void print(double);

    void println(const char[]);
    void println(char);
    void println(unsigned char, int = DEC);
    void println(int, int = DEC);
    void println(unsigned int, int = DEC);
    void println(long, int = DEC);
    void println(unsigned long, int = DEC);
    void println(long long, int = DEC);
    void println(unsigned long long, int = DEC);
    void println(double);
    void println(void);

    void printHex(const char* suffix, const uint8_t* data, size_t length, bool newline = true);
#else
    /* Use silent inline overloads, NOT macros.
     * Macros like #define print(...) expand identifiers inside member-function
     * call expressions (e.g. Serial.print("x") → Serial.do{}while(0)) which
     * causes "expected unqualified-id" errors in user sketches. */
    static inline void print(const char*)                                     {}
    static inline void print(char)                                            {}
    static inline void print(unsigned char, int = DEC)                        {}
    static inline void print(int, int = DEC)                                  {}
    static inline void print(unsigned int, int = DEC)                         {}
    static inline void print(long, int = DEC)                                 {}
    static inline void print(unsigned long, int = DEC)                        {}
    static inline void print(long long, int = DEC)                            {}
    static inline void print(unsigned long long, int = DEC)                   {}
    static inline void print(double)                                           {}
    static inline void println(void)                                           {}
    static inline void println(const char*)                                    {}
    static inline void println(char)                                           {}
    static inline void println(unsigned char, int = DEC)                      {}
    static inline void println(int, int = DEC)                                {}
    static inline void println(unsigned int, int = DEC)                       {}
    static inline void println(long, int = DEC)                               {}
    static inline void println(unsigned long, int = DEC)                      {}
    static inline void println(long long, int = DEC)                          {}
    static inline void println(unsigned long long, int = DEC)                 {}
    static inline void println(double)                                         {}
    static inline void printHex(const char*, const uint8_t*, size_t, bool = true) {}
#endif

#ifdef KNX_ACTIVITYCALLBACK
    #define KNX_ACTIVITYCALLBACK_DIR 0x00
    #define KNX_ACTIVITYCALLBACK_DIR_RECV 0x00
    #define KNX_ACTIVITYCALLBACK_DIR_SEND 0x01
    #define KNX_ACTIVITYCALLBACK_IPUNICAST 0x02
    #define KNX_ACTIVITYCALLBACK_NET 0x04
#endif

const uint8_t* popByte(uint8_t& b, const uint8_t* data);
const uint8_t* popWord(uint16_t& w, const uint8_t* data);
const uint8_t* popInt(uint32_t& i, const uint8_t* data);
const uint8_t* popByteArray(uint8_t* dst, uint32_t size, const uint8_t* data);
uint8_t* pushByte(uint8_t b, uint8_t* data);
uint8_t* pushWord(uint16_t w, uint8_t* data);
uint8_t* pushInt(uint32_t i, uint8_t* data);
uint8_t* pushByteArray(const uint8_t* src, uint32_t size, uint8_t* data);
uint16_t getWord(const uint8_t* data);
uint32_t getInt(const uint8_t* data);

void sixBytesFromUInt64(uint64_t num, uint8_t* toByteArray);
uint64_t sixBytesToUInt64(uint8_t* data);

uint16_t crc16Ccitt(uint8_t* input, uint16_t length);
uint16_t crc16Dnp(uint8_t* input, uint16_t length);

enum ParameterFloatEncodings
{
    Float_Enc_DPT9 = 0,          // 2 Byte. See Chapter 3.7.2 section 3.10 (Datapoint Types 2-Octet Float Value)
    Float_Enc_IEEE754Single = 1, // 4 Byte. C++ float
    Float_Enc_IEEE754Double = 2, // 8 Byte. C++ double
};


#if defined(ARDUINO_ARCH_SAMD)
    // temporary undef until framework-arduino-samd > 1.8.9 is released. See https://github.com/arduino/ArduinoCore-samd/pull/399 for a PR should will probably address this
    #undef max
    #undef min
    // end of temporary undef
#endif
