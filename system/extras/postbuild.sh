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

# A sketch that keeps KNX data in flash owns Bank 2 Sector 6 (0x081C0000, see
# OpenPLC_KNX/src/knx_config.h), so its image must end below that sector:
# 0x081C0000 - 0x08020000 = 1703936 bytes. Other sketches keep the full area.
BIN_FILE="$BUILD_PATH/$PROJECT_NAME.bin"
KNX_IMAGE_LIMIT=1703936
if [ -f "$ELF_FILE" ] && [ -f "$BIN_FILE" ] && \
   "${COMPILER_PATH}arm-none-eabi-nm" "$ELF_FILE" 2>/dev/null | grep -q ' knx_nvm_sector_write$'; then
  BIN_SIZE=$(wc -c < "$BIN_FILE" | tr -d ' ')
  if [ "$BIN_SIZE" -gt "$KNX_IMAGE_LIMIT" ]; then
    echo "error: this sketch uses OpenPLC_KNX, whose data lives in flash at 0x081C0000." >&2
    echo "error: the image is $BIN_SIZE bytes; with KNX it must not exceed $KNX_IMAGE_LIMIT bytes (1664 KiB)." >&2
    exit 1
  fi
fi

# Copy the correct openocd.cfg if exists
if [ ! -f "$BOARD_PLATFORM_PATH/variants/$BUILD_SERIE/openocd.cfg" ]; then
  printf 'No %s available. Debug is not supported.' "$BOARD_PLATFORM_PATH/variants/$BUILD_SERIE/openocd.cfg"
else
  cp -f "$BOARD_PLATFORM_PATH/variants/$BUILD_SERIE/openocd.cfg" "$BUILD_PATH"
fi
