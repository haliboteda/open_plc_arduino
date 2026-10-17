#pragma once

/*
 * Dependency-scanner hook only: system/extras/prebuild.sh force-includes this
 * header so arduino-cli compiles this library and STM32duino_LwIP into every
 * sketch. The core's declarations are in cores/arduino/stm32/openplc_net.h;
 * sketches use the Net object.
 */
#include <LwIP.h>
