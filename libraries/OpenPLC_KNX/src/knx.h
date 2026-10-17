#pragma once
/*
 * knx.h - top-level include for the KNX stack (STM32H743 / OpenPLC build).
 *
 * Include this header to access KnxFacade, GroupObject, DPT types, and all
 * BAU variants.  The platform (Stm32H743OpenPLCPlatform) is pulled in
 * automatically via knx_facade.h.
 *
 * Sketches include <OpenPLC_KNX.h> (which includes this) and use the KNX
 * object it declares.
 */
#include "knx_facade.h"
