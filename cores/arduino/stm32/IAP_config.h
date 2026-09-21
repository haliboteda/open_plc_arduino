#ifndef IAP_CONFIG_H
#define IAP_CONFIG_H

/* Values shared between the IAP command channels. */
#define CDC_RX_BUFFER_SIZE  (32U * 1024U)

#ifndef OPENPLC_SERVER_PORT
#define OPENPLC_SERVER_PORT 56865
#endif

#ifndef OPENPLC_DEVICE_NAME
#define OPENPLC_DEVICE_NAME "STM32H743"
#endif

/* The board package's release version -- the FOURTH field of the identity
 * string. It comes from build.fw_version in boards.txt (-DOPENPLC_FW_VERSION)
 * and is the same for every sketch built with this package.
 *
 * Not the sketch's own version. That is openplc_app_version, the fifth field,
 * which the sketch declares with OPENPLC_APP_VERSION() and postbuild.sh reads
 * back out of the ELF into <image>.version for the upload tool to compare.
 * See cores/arduino/openplc_app_version.h.
 *
 * No fallback on purpose. The only build path that fails to set this is
 * tools/platformio/platformio-build.py, which is upstream STM32duino code this
 * project has never maintained and does not support; a silent "0.0.0" there
 * would look like a real version in the identity string rather than a missing
 * one. Failing the build says so instead. */
#ifndef OPENPLC_FW_VERSION
#error "OPENPLC_FW_VERSION is not set - it comes from build.fw_version in boards.txt. The PlatformIO build path does not set it and is not supported."
#endif

/* Role, the third field of the identity string. The bootloader defines this as
 * "BOOTLD" -- the two must differ, that is how a PC tool tells a running
 * application from the bootloader. */
#ifndef UDP_SERVER_NAME
#define UDP_SERVER_NAME "CUSAPP"
#endif

/* The baud rate the PC tool opens the CDC port at to ask for upload mode. Not a
 * flag -- this one stays. */
#ifndef MAGIC_CDC_RATE
#define MAGIC_CDC_RATE 1200U
#endif

/* MAGIC_APP_FLAG / MAGIC_ETH_FLAG / MAGIC_CDC_FLAG / MAGIC_BKP_REG are gone.
 * The boot-mode request no longer lives in an RTC backup register; see
 * IAP_boot_handoff.h for the replacement and for why the register was the wrong
 * place for it. Keep this file in step with the bootloader's
 * open_plc_cube_ide/Core/Inc/IAP_config.h. */

#endif /* IAP_CONFIG_H */