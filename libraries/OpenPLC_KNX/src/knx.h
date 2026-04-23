#pragma once
/*
 * knx.h — top-level include for the KNX stack (STM32H743 / OpenPLC build).
 *
 * Include this header to access KnxFacade, GroupObject, DPT types, and all
 * BAU variants.  The platform (Stm32H743OpenPLCPlatform) is pulled in
 * automatically via knx_facade.h.
 *
 * Typical use in user code:
 *   #include <OpenPLC_KNX.h>   // includes knx.h transitively
 */
#include "knx_facade.h"
