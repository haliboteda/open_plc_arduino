/* Host stand-in for the core's stm32_def.h: only the chip UID that
 * openplc_calib.c compares against, and the RAM area it reads instead of
 * flash. */
#ifndef STM32_DEF_H_STUB
#define STM32_DEF_H_STUB

#include <stdint.h>

extern uint8_t test_calib_area[];
extern const uint32_t test_uid[3];

static inline uint32_t HAL_GetUIDw0(void) { return test_uid[0]; }
static inline uint32_t HAL_GetUIDw1(void) { return test_uid[1]; }
static inline uint32_t HAL_GetUIDw2(void) { return test_uid[2]; }

#endif
