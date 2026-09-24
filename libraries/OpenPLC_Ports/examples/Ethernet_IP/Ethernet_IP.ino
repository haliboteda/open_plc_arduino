/*
 * Ethernet_IP -- the Ethernet port.
 *
 * What it does: prints the link state and the address the board got from
 * DHCP, whenever either changes.
 * Connect: an Ethernet cable to a network with a DHCP server.
 * Expect: "link up", then an address. From a PC on the same network,
 *   ping that address, or look for the board under Tools > Port in the IDE.
 * Serial Monitor: the board's USB port, 115200 baud.
 */

#include <OpenPLC_Net_Autostart.h>

OPENPLC_APP_VERSION(1, 0, 0);

int last_link = -1;
bool shown_ip = false;

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}
  Serial.println("Ethernet_IP: waiting for link and address.");
}

void loop() {
  int link = openplc_net_link_up();
  if (link != last_link) {
    Serial.println(link ? "link up" : "link down");
    last_link = link;
    shown_ip = false;
  }
  unsigned char ip[4];
  if (!shown_ip && openplc_net_get_ipv4(ip)) {
    Serial.print("address ");
    for (int i = 0; i < 4; i++) {
      Serial.print(ip[i]);
      Serial.print(i < 3 ? "." : "\n");
    }
    shown_ip = true;
  }
  delay(200);
}
