#include "ethernetif.h"

#include "lwip/init.h"
#include "lwip/netif.h"
#include "netif/ethernet.h"
#include "lwip/timeouts.h"
#include "lwip/dhcp.h"
#include "lwip/ip4_addr.h"

struct netif gnetif;

static uint32_t ethernetLinkTimer = 0;
static uint8_t netInited = 0;

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

