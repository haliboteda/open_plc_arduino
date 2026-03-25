#ifndef IAP_CONFIG_H
#define IAP_CONFIG_H

/* Shared magic values used for boot flags and CDC behavior. */
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

#ifndef MAGIC_CDC_RATE
#define MAGIC_CDC_RATE 1200U
#endif

#ifndef MAGIC_APP_FLAG
#define MAGIC_APP_FLAG 0xAAU
#endif

#ifndef MAGIC_ETH_FLAG
#define MAGIC_ETH_FLAG 0xAEU
#endif

#ifndef MAGIC_CDC_FLAG
#define MAGIC_CDC_FLAG 0xAFU
#endif

#ifndef MAGIC_BKP_REG
#define MAGIC_BKP_REG RTC_BKP_DR0
#endif

#endif /* IAP_CONFIG_H */