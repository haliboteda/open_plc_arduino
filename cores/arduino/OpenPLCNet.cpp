#include "Arduino.h"

#ifdef OPENPLC_UDP_SERVER_AUTOSTART
#include "OpenPLCNet.h"
#include "openplc_net.h"

bool OpenPLCNet::linkUp()
{
  return openplc_net_link_up() != 0U;
}

bool OpenPLCNet::hasIP()
{
  return openplc_net_has_ip() != 0U;
}

IPAddress OpenPLCNet::localIP()
{
  uint8_t ip[4] = {0};
  (void)openplc_net_get_ipv4(ip);
  return IPAddress(ip);
}

uint8_t *OpenPLCNet::macAddress(uint8_t *mac)
{
  (void)openplc_net_get_mac(mac);
  return mac;
}
#endif
