#include "ethernetif.h"
#include "Arduino.h"

#include "lwip/init.h"
#include "lwip/netif.h"
#include "netif/ethernet.h"
#include "lwip/timeouts.h"
#include "lwip/dhcp.h"
#include "lwip/ip4_addr.h"
#include "lwip/stats.h"
#if LWIP_IGMP
#include "lwip/igmp.h"
#endif

struct netif gnetif;

static uint32_t ethernetLinkTimer = 0;
static uint8_t netInited = 0;
/* Set to 1 once IGMP REPORTs have been re-sent with the real IP address.
 * igmp_joingroup() is called before DHCP completes (IP = 0.0.0.0), so the
 * IGMP REPORT carries a source of 0.0.0.0 - multicast switches may not
 * record it.  Re-sending once DHCP assigns an address fixes discovery. */
static uint8_t igmpRefreshed = 0;

static void ethernet_link_status_updated(struct netif *netif)
{
  (void)netif;
}

void openplc_net_init(void)
{
  if (netInited) {
    return;
  }

  ip4_addr_t ipaddr = {0};
  ip4_addr_t netmask = {0};
  ip4_addr_t gw = {0};

  lwip_init();
  netif_add(&gnetif, &ipaddr, &netmask, &gw, NULL, &ethernetif_init, &ethernet_input);
  netif_set_default(&gnetif);
  netif_set_up(&gnetif);
  netif_set_link_callback(&gnetif, ethernet_link_status_updated);
  dhcp_start(&gnetif);

  netInited = 1;
}

void openplc_net_process(void)
{
  if (!netInited) {
    return;
  }

  if (ethernetif_rx_event_pending()) {
    ethernetif_rx_event_clear();
    ethernetif_input(&gnetif);
  }
  sys_check_timeouts();

  if ((HAL_GetTick() - ethernetLinkTimer) >= 100U) {
    ethernetLinkTimer = HAL_GetTick();
    ethernet_link_check_state(&gnetif);
  }

  /* Once DHCP assigns a real IP, re-send IGMP REPORTs so network switches
   * record the group membership from a valid source address. */
#if LWIP_IGMP
  if (!igmpRefreshed && dhcp_supplied_address(&gnetif)) {
    igmp_report_groups(&gnetif);
    igmpRefreshed = 1U;
  }
#endif
}

const ip_addr_t *openplc_net_ip_addr(void)
{
  static ip_addr_t out;

  if (!netInited) {
    return NULL;
  }

  ip_addr_copy_from_ip4(out, *netif_ip4_addr(&gnetif));
  return &out;
}

uint8_t openplc_net_link_up(void)
{
  if (!netInited) {
    return 0U;
  }
  return (uint8_t)(netif_is_link_up(&gnetif) ? 1U : 0U);
}

uint8_t openplc_net_has_ip(void)
{
  if (!netInited) {
    return 0U;
  }
  return (uint8_t)(dhcp_supplied_address(&gnetif) ? 1U : 0U);
}

uint8_t openplc_net_get_ipv4(uint8_t out[4])
{
  if ((out == NULL) || (!netInited) || (!dhcp_supplied_address(&gnetif))) {
    return 0U;
  }
  const ip4_addr_t *ip = netif_ip4_addr(&gnetif);
  out[0] = (uint8_t)ip4_addr1(ip);
  out[1] = (uint8_t)ip4_addr2(ip);
  out[2] = (uint8_t)ip4_addr3(ip);
  out[3] = (uint8_t)ip4_addr4(ip);
  return 1U;
}

uint8_t openplc_lwip_stats_enabled(void)
{
#if LWIP_STATS
  return 1U;
#else
  return 0U;
#endif
}

uint32_t openplc_lwip_mem_avail(void)
{
#if LWIP_STATS && MEM_STATS
  return (uint32_t)lwip_stats.mem.avail;
#else
  return 0U;
#endif
}

uint32_t openplc_lwip_mem_used(void)
{
#if LWIP_STATS && MEM_STATS
  return (uint32_t)lwip_stats.mem.used;
#else
  return 0U;
#endif
}

uint32_t openplc_lwip_mem_max(void)
{
#if LWIP_STATS && MEM_STATS
  return (uint32_t)lwip_stats.mem.max;
#else
  return 0U;
#endif
}

uint16_t openplc_lwip_udp_pcb_used(void)
{
#if LWIP_STATS && MEMP_STATS
  return (uint16_t)lwip_stats.memp[MEMP_UDP_PCB]->used;
#else
  return 0U;
#endif
}

uint16_t openplc_lwip_udp_pcb_max(void)
{
#if LWIP_STATS && MEMP_STATS
  return (uint16_t)lwip_stats.memp[MEMP_UDP_PCB]->max;
#else
  return 0U;
#endif
}

uint16_t openplc_lwip_udp_pcb_err(void)
{
#if LWIP_STATS && MEMP_STATS
  return (uint16_t)lwip_stats.memp[MEMP_UDP_PCB]->err;
#else
  return 0U;
#endif
}

uint16_t openplc_lwip_pbuf_used(void)
{
#if LWIP_STATS && MEMP_STATS
  return (uint16_t)lwip_stats.memp[MEMP_PBUF]->used;
#else
  return 0U;
#endif
}

uint16_t openplc_lwip_pbuf_max(void)
{
#if LWIP_STATS && MEMP_STATS
  return (uint16_t)lwip_stats.memp[MEMP_PBUF]->max;
#else
  return 0U;
#endif
}

uint16_t openplc_lwip_pbuf_err(void)
{
#if LWIP_STATS && MEMP_STATS
  return (uint16_t)lwip_stats.memp[MEMP_PBUF]->err;
#else
  return 0U;
#endif
}

uint16_t openplc_lwip_igmp_group_used(void)
{
#if LWIP_STATS && MEMP_STATS && LWIP_IGMP
  return (uint16_t)lwip_stats.memp[MEMP_IGMP_GROUP]->used;
#else
  return 0U;
#endif
}

uint16_t openplc_lwip_igmp_group_max(void)
{
#if LWIP_STATS && MEMP_STATS && LWIP_IGMP
  return (uint16_t)lwip_stats.memp[MEMP_IGMP_GROUP]->max;
#else
  return 0U;
#endif
}

uint16_t openplc_lwip_igmp_group_err(void)
{
#if LWIP_STATS && MEMP_STATS && LWIP_IGMP
  return (uint16_t)lwip_stats.memp[MEMP_IGMP_GROUP]->err;
#else
  return 0U;
#endif
}
