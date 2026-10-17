/*
 * Ethernet_IP -- the Ethernet port.
 *
 * What it does: prints the link state and the address the board got from
 * DHCP, whenever either changes.
 * Connect: an Ethernet cable to a network with a DHCP server.
 * Expect: "link up", then an address. From a PC on the same network,
 *   ping that address, or look for the board under Tools > Port in the IDE.
 * Serial Monitor: on the board's USB port.
 */

OPENPLC_APP_VERSION(1, 0, 0);

int last_link = -1;
bool shown_ip = false;

void printBanner() {
  Serial.println("Ethernet_IP: waiting for link and address.");
  last_link = -1;  // and the current link and address right after it
}

// Prints the description each time the Serial Monitor connects, not once in
// setup(): every reset re-enumerates USB and drops what setup() printed (README).
void printBannerOnConnect() {
  static bool wasConnected = false;
  static uint32_t lastCheck = 0;
  if (millis() - lastCheck < 250) {
    return;  // (bool)Serial takes 10 ms on USB
  }
  lastCheck = millis();
  bool now = (bool)Serial;
  if (now && !wasConnected) {
    printBanner();
  }
  wasConnected = now;
}

void setup() {
  Serial.begin(115200);
}

void loop() {
  printBannerOnConnect();
  int link = Net.linkUp();
  if (link != last_link) {
    Serial.println(link ? "link up" : "link down");
    last_link = link;
    shown_ip = false;
  }
  if (!shown_ip && Net.hasIP()) {
    Serial.print("address ");
    Serial.println(Net.localIP());
    shown_ip = true;
  }
  delay(200);
}
