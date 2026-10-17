/*
 * openplc_syslog.h
 *
 * The core's system log. Every line goes into the SRAM4 boot log
 * (stm32/boot_log.h) and, while the host holds USB port 1 open (DTR set), out
 * on SerialUSB; nothing goes to the RS232 terminals. Requirements R3-22 and
 * R3-23 in $PROD/docs/modules/M3-app-runtime.md, line format in
 * $PROD/docs/modules/M1/BOOT-LOG.md.
 */

#ifndef OPENPLC_SYSLOG_H_
#define OPENPLC_SYSLOG_H_

#include <stddef.h>
#include <stdint.h>
#include "boot_log.h"

/* Library tags not in boot_log.h, which is mirrored with the bootloader. */
#define OPENPLC_SYSLOG_TAG_KNX  "[sys][KNX] "

#ifdef __cplusplus
extern "C" {
#endif

/* 1 = send to USB port 1. The core's weak default is 1; a sketch overrides it
 * with OPENPLC_SYSTEM_LOG(OFF). Either way every line still goes to the
 * buffer. */
extern const uint8_t openplc_system_log_on;

/* One line: tag (a BOOT_LOG_TAG_* string) followed by text. A text that
 * already starts with '[' keeps its own tag after "[sys]" and tag is unused.
 * Safe from interrupt and fault context; there it only reaches the buffer. */
void openplc_syslog_write(const char *tag, const char *text, size_t len);

/* stdout / stderr bytes from _write(): collected into lines, untagged lines
 * get BOOT_LOG_TAG_NET (every core and library printf today is network code). */
void openplc_syslog_stdout(const char *ptr, size_t len);

/* Sends records not yet sent to USB port 1 while DTR is set, then marks them
 * sent. Main context only; called from the main loop and yield(). Sends
 * nothing when DTR is clear or the log is OFF. Also runs the heartbeat LED
 * blink on a DTR rising edge (R3-25), whether the log is ON or OFF. */
void openplc_syslog_service(void);

#ifdef __cplusplus
}

#include "Print.h"

/* A Print that turns what is printed into system log lines, one per '\n'. */
class SystemLogPrint : public Print {
  public:
    explicit SystemLogPrint(const char *tag) : _tag(tag), _len(0) {}
    size_t write(uint8_t c) override;
    using Print::write;

  private:
    const char *_tag;
    size_t _len;
    char _line[BOOT_LOG_LINE_MAX];
};

/*
 * Write once near the top of the sketch to keep system log lines off USB
 * port 1; leaving it out means ON:
 *
 *     OPENPLC_SYSTEM_LOG(OFF);
 *
 * Same shape as OPENPLC_APP_VERSION(): it defines the symbol the core
 * declares, so the choice is fixed when the sketch is linked. extern "C"
 * gives the const object external linkage; anything but ON or OFF fails to
 * compile.
 */
#define OPENPLC_SYSTEM_LOG_STATE_ON   1
#define OPENPLC_SYSTEM_LOG_STATE_OFF  0
#define OPENPLC_SYSTEM_LOG(state)                                             \
    extern "C" const uint8_t openplc_system_log_on                            \
        __attribute__((used)) = OPENPLC_SYSTEM_LOG_STATE_##state

#endif /* __cplusplus */

#endif /* OPENPLC_SYSLOG_H_ */
