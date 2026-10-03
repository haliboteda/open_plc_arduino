#!/bin/bash

BUILD_PATH="$1"
BUILD_SERIE="$2"
BOARD_PLATFORM_PATH="$3"
COMPILER_PATH="$4"
PROJECT_NAME="$5"

# Leave the sketch's version next to the binary for the upload tool, which
# compares it against the version the board reports over UDP discovery.
#
# Read it out of the ELF rather than out of the sketch source: whatever lands
# here is then, by construction, the same bytes that are in the firmware.
# Parsing the source would let the two drift (conditional compilation, a
# commented-out older line) with nothing to catch it.
ELF_FILE="$BUILD_PATH/$PROJECT_NAME.elf"
VERSION_FILE="$BUILD_PATH/$PROJECT_NAME.version"
if [ -f "$ELF_FILE" ]; then
  "${COMPILER_PATH}arm-none-eabi-objcopy" -O binary -j .openplc_version       "$ELF_FILE" "$VERSION_FILE" 2>/dev/null || true
fi

# Copy the correct openocd.cfg if exists
if [ ! -f "$BOARD_PLATFORM_PATH/variants/$BUILD_SERIE/openocd.cfg" ]; then
  printf 'No %s available. Debug is not supported.' "$BOARD_PLATFORM_PATH/variants/$BUILD_SERIE/openocd.cfg"
else
  cp -f "$BOARD_PLATFORM_PATH/variants/$BUILD_SERIE/openocd.cfg" "$BUILD_PATH"
fi
