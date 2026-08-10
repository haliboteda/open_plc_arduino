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
const void *openplc_net_ip_addr(void);
unsigned char openplc_net_link_up(void);
unsigned char openplc_net_has_ip(void);
unsigned char openplc_net_get_ipv4(unsigned char out[4]);
unsigned char openplc_lwip_stats_enabled(void);
unsigned long openplc_lwip_mem_avail(void);
unsigned long openplc_lwip_mem_used(void);
unsigned long openplc_lwip_mem_max(void);
unsigned short openplc_lwip_udp_pcb_used(void);
unsigned short openplc_lwip_udp_pcb_max(void);
unsigned short openplc_lwip_udp_pcb_err(void);
unsigned short openplc_lwip_pbuf_used(void);
unsigned short openplc_lwip_pbuf_max(void);
unsigned short openplc_lwip_pbuf_err(void);
unsigned short openplc_lwip_igmp_group_used(void);
unsigned short openplc_lwip_igmp_group_max(void);
unsigned short openplc_lwip_igmp_group_err(void);

#ifdef __cplusplus
}
#endif
