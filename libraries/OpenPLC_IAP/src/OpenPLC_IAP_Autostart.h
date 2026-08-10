#pragma once

/*
 * Force Arduino's dependency scanner to pull in this library's sources
 * (udp_server.c, iap_auth.c, iap_keyderive.c, sha256.c). cores/arduino's
 * main.cpp calls openplc_udp_server_start() unconditionally (behind
 * OPENPLC_UDP_SERVER_AUTOSTART, always defined for this board) using its
 * own local extern "C" prototypes -- it never includes this header. The
 * only thing that makes those symbols actually get compiled and linked in
 * is system/extras/prebuild.sh force-including this header (alongside
 * OpenPLC_Net_Autostart.h) into every sketch build. If this library is
 * ever renamed or these declarations moved, prebuild.sh must be updated
 * too, or every sketch fails to link with "undefined reference to
 * openplc_udp_server_start".
 */

#ifdef __cplusplus
extern "C" {
#endif

void openplc_udp_server_start(void (*reboot_cb)(void));
void openplc_udp_server_stop(void);
unsigned long openplc_udp_server_start_count(void);
unsigned long openplc_udp_server_recv_count(void);
unsigned long openplc_udp_server_reply_count(void);
unsigned long openplc_udp_server_bind_fail_count(void);
unsigned long openplc_udp_server_last_rx_tick(void);
unsigned short openplc_udp_server_last_rx_port(void);
unsigned short openplc_udp_server_last_rx_len(void);

#ifdef __cplusplus
}
#endif
