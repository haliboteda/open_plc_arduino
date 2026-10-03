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
