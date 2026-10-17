#ifndef OPENPLC_NET_H
#define OPENPLC_NET_H

/* Core-internal: the network and IAP UDP service the core starts before
 * setup(). Sketches use the Net object instead ($PROD/docs/modules/M3/USER-API.md). */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* OpenPLC_Net */
void openplc_net_init(void);
void openplc_net_process(void);
uint8_t openplc_net_link_up(void);
uint8_t openplc_net_has_ip(void);
uint8_t openplc_net_get_ipv4(uint8_t out[4]);
uint8_t openplc_net_get_mac(uint8_t out[6]);

/* OpenPLC_IAP */
void openplc_udp_server_start(void);
uint32_t openplc_udp_server_recv_count(void);
uint32_t openplc_udp_server_last_rx_tick(void);
uint16_t openplc_udp_server_last_rx_len(void);

#ifdef __cplusplus
}
#endif

#endif
