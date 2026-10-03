#!/bin/bash

BUILD_PATH="$1"
BUILD_SOURCE_PATH="$2"
BOARD_PLATFORM_PATH="$3"

# Every sketch must declare its version. The upload tool refuses to flash a
# board with firmware older than what it already runs, and it needs a version
# to compare against.
#
# Deliberately a loose check: it looks for the word, it does not parse the
# value. The linker is what actually enforces this -- the core declares
# openplc_app_version without defining it -- so a false pass here is caught a
# few seconds later, while a false fail would block a correct sketch.
if ! grep -l "OPENPLC_APP_VERSION" "$BUILD_SOURCE_PATH"/*.ino >/dev/null 2>&1; then
  echo "" >&2
  echo "error: this sketch has no version number." >&2
  echo "" >&2
  echo "  Add this line near the top of your sketch:" >&2
  echo "" >&2
  echo "      OPENPLC_APP_VERSION(1, 0, 0);" >&2
  echo "" >&2
  echo "  Without it the board cannot tell whether an upload is newer or" >&2
  echo "  older than the firmware it is already running." >&2
  echo "" >&2
  exit 1
fi

# Create sketch dir if not exists
if [ ! -f "$BUILD_PATH/sketch" ]; then
  mkdir -p "$BUILD_PATH/sketch"
fi

# Create empty build.opt if build_opt.h does not exists in the original sketch dir
# Then add or append -fmacro-prefix-map option to change __FILE__ absolute path of
# the board platform folder to a relative path by using '.'.
# (i.e. the folder containing boards.txt)
if [ ! -f "$BUILD_SOURCE_PATH/build_opt.h" ]; then
  printf '\n-fmacro-prefix-map="%s"=.' "${BOARD_PLATFORM_PATH//\\/\\\\}" > "$BUILD_PATH/sketch/build.opt"
else
  # Else copy the build_opt.h as build.opt
  # Workaround to the header file preprocessing done by arduino-cli
  # See https://github.com/arduino/arduino-cli/issues/1338
  cp "$BUILD_SOURCE_PATH/build_opt.h" "$BUILD_PATH/sketch/build.opt"
  printf '\n-fmacro-prefix-map="%s"=.' "${BOARD_PLATFORM_PATH//\\/\\\\}" >> "$BUILD_PATH/sketch/build.opt"
fi

# Force include of SrcWrapper, OpenPLC_Net, OpenPLC_IAP and LwIP to ensure
# library linking. main.cpp calls openplc_udp_server_start()/etc.
# unconditionally using its own local extern "C" prototypes -- it never
# includes OpenPLC_IAP_Autostart.h itself, so this force-include is the only
# thing that makes the Arduino dependency scanner compile OpenPLC_IAP's
# sources (udp_server.c, iap_auth.c, iap_keyderive.c, sha256.c) into the
# build at all. Removing this line breaks every sketch's link step.
cat > "$BUILD_PATH/sketch/SrcWrapper.cpp" <<'EOC'
#include <SrcWrapper.h>
#include <OpenPLC_Net_Autostart.h>
#include <OpenPLC_IAP_Autostart.h>
EOC
