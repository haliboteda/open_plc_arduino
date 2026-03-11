#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void openplc_net_init(void);
void openplc_net_process(void);
void openplc_udp_server_start(void (*reboot_cb)(void));

#ifdef __cplusplus
}
#endif
