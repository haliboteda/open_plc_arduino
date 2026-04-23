#include "knx_facade.h"
#include "knx/bits.h"

/*
 * knx_facade.cpp — stripped for STM32H743 / OpenPLC only.
 *
 * KNX_NO_AUTOMATIC_GLOBAL_INSTANCE is always set (defined in
 * stm32h743_openplc_platform.h), so no global `knx` instance is created
 * here.  The application uses the `KNX` instance defined in OpenPLC_KNX.cpp.
 */
