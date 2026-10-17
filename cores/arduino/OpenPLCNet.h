#ifndef OPENPLCNET_H
#define OPENPLCNET_H

#include "IPAddress.h"

/* The Ethernet port. The core brings the network up before setup() because
 * IAP needs it, so there is no begin(). */
class OpenPLCNet {
  public:
    bool linkUp();
    bool hasIP();
    IPAddress localIP();   /* 0.0.0.0 until DHCP has given an address */
    uint8_t *macAddress(uint8_t *mac);
};

#endif
