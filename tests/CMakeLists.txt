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
