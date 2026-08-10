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

#ifndef OPENPLC_CUSAPP_VERSION
#define OPENPLC_CUSAPP_VERSION "0.1.2"
#endif

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