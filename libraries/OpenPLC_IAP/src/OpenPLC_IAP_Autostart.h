#pragma once

/*
 * Dependency-scanner hook only: system/extras/prebuild.sh force-includes this
 * header so arduino-cli compiles this library into every sketch; main.cpp
 * calls openplc_udp_server_start() (declared in cores/arduino/stm32/openplc_net.h).
 * Rename this library or this header and prebuild.sh must follow, or every
 * sketch fails to link.
 */
