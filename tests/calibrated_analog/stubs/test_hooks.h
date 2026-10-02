/* Force-included into every T3-07 source: the calibration area and the log
 * line go to the test instead of flash and printf. */
#ifndef TEST_HOOKS_H
#define TEST_HOOKS_H

#include <stdint.h>

extern uint8_t test_calib_area[];
void test_log(const char *msg);

#define CALIB_AREA_ADDR ((uintptr_t)test_calib_area)
#define OPENPLC_LOG_LINE(msg) test_log(msg)

#endif
