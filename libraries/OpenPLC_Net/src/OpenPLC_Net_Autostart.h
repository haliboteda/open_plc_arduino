#pragma once

/*
 * Force Arduino's dependency scanner to pull in STM32duino_LwIP sources.
 * The header is intentionally empty and exists only as the LwIP library
 * entry-point for build-system discovery.
 */
#include <LwIP.h>

#ifdef __cplusplus
extern "C" {
#endif

void openplc_net_init(void);
void openplc_net_process(void);
void openplc_udp_server_start(void (*reboot_cb)(void));

#ifdef __cplusplus
}
#endif
