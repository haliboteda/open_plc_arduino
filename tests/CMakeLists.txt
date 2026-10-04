# Host-side C tests for the board package: real library sources + stubs, built
# with the PC's own compiler. Standalone project -- configure this directory
# (cmake -S tests); it is not part of any Arduino build.
cmake_minimum_required(VERSION 3.20)
project(core_host_tests C)

set(CORE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." CACHE PATH "board package repo root")
set(IAP "${CORE_ROOT}/libraries/OpenPLC_IAP/src")
if(NOT EXISTS "${IAP}/owner_root_ro.c")
  message(FATAL_ERROR "Not the board package repo: ${CORE_ROOT}")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -O0 -g)
enable_testing()

# --- T2-21: the root in force cannot revoke itself ---------------------------
add_executable(owner_revoke_test
  owner_revoke/stubs/iap_keyderive_stub.c
  ${IAP}/owner_root_ro.c ${IAP}/sha256.c ${IAP}/fw_verify.c ${IAP}/uecc/uECC.c
  owner_revoke/test_main.c)
target_include_directories(owner_revoke_test PRIVATE owner_revoke/stubs ${IAP})
# Seen before owner_root_ro.c's own default, so the record area is a RAM buffer.
target_compile_options(owner_revoke_test PRIVATE
  -include ${CMAKE_CURRENT_SOURCE_DIR}/owner_revoke/stubs/fake_owner_area.h)
add_test(NAME T2-21 COMMAND owner_revoke_test)

# --- T3-07: AI / AO in mV and mA apply the calibration, or fall back ---------
set(PORTS "${CORE_ROOT}/libraries/OpenPLC_Ports/src")
add_executable(calib_test
  ${PORTS}/openplc_calib.c ${PORTS}/openplc_analog.c calibrated_analog/test_main.c)
target_include_directories(calib_test PRIVATE calibrated_analog/stubs ${PORTS})
# test_hooks.h redirects the calibration area and the log line to the test.
target_compile_options(calib_test PRIVATE
  -include ${CMAKE_CURRENT_SOURCE_DIR}/calibrated_analog/stubs/test_hooks.h)
target_link_libraries(calib_test PRIVATE m)
foreach(c ok blank corrupt other)
  add_test(NAME T3-07.${c} COMMAND calib_test ${c})
endforeach()

# --- T3-08: the reset cause the bootloader publishes reaches the sketch -------
add_executable(reset_cause_test
  ${CORE_ROOT}/cores/arduino/stm32/IAP_boot_handoff.c ${PORTS}/openplc_reset.c
  reset_cause/test_main.c)
target_include_directories(reset_cause_test PRIVATE
  reset_cause/stubs ${PORTS} ${CORE_ROOT}/cores/arduino/stm32)
target_compile_options(reset_cause_test PRIVATE
  -include ${CMAKE_CURRENT_SOURCE_DIR}/reset_cause/stubs/test_hooks.h)
foreach(c watchdog poweron software pin corrupt blank)
  add_test(NAME T3-08.${c} COMMAND reset_cause_test ${c})
endforeach()

# --- T3-09: the KNX TP1 data link engine on a simulated bus --------------------
set(KNX "${CORE_ROOT}/libraries/OpenPLC_KNX/src")
add_executable(knx_tp1_test ${KNX}/stknx_tp1.c knx_tp1/test_main.c)
target_include_directories(knx_tp1_test PRIVATE ${KNX})
foreach(c codec rx_ack rx_other rx_nak rx_busy tx_ack tx_priority tx_noack tx_busy collision)
  add_test(NAME T3-09.${c} COMMAND knx_tp1_test ${c})
endforeach()

# --- T3-10: the YMODEM receiver against scripted senders -----------------------
add_executable(ymodem_test ${PORTS}/openplc_ymodem.c ymodem/test_main.c)
target_include_directories(ymodem_test PRIVATE ${PORTS})
foreach(c one_file short_packets bad_crc duplicate cancel idle noise refuse short_file)
  add_test(NAME T3-10.${c} COMMAND ymodem_test ${c})
endforeach()

# --- T3-12: OpenPLC_SD's file operations on the real FatFs over a RAM disk ------
enable_language(CXX)
set(SDLIB "${CORE_ROOT}/libraries/OpenPLC_SD/src")
set(FATFS "${CORE_ROOT}/libraries/FatFs/src")
add_executable(sd_lib_test
  ${SDLIB}/OpenPLC_SD.cpp
  ${FATFS}/ff.c ${FATFS}/ffunicode.c ${FATFS}/ffsystem.c ${FATFS}/diskio.c ${FATFS}/ff_gen_drv.c
  sd_lib/test_main.cpp)
# stubs first: its Arduino.h and ffconf.h stand in for the STM32 ones.
target_include_directories(sd_lib_test PRIVATE sd_lib/stubs ${SDLIB} ${FATFS})
target_compile_definitions(sd_lib_test PRIVATE OPENPLC_SD_HOST_TEST)
foreach(c write_read append seek_peek remove_exists dirs root unmounted)
  add_test(NAME T3-12.${c} COMMAND sd_lib_test ${c})
endforeach()
