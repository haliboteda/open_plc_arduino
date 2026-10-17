/*
 * KNX_IPRouter -- the board as a KNX IP router: telegrams pass between the
 * KNX TP bus and KNXnet/IP on the Ethernet port.
 *
 * Board: the KNX TP bus, with its power
 *   supply, on the KNX terminals; the Ethernet port on the same network as
 *   the ETS PC.
 * ETS: press the BOOT0 button once while this sketch runs (the system LED
 *   stays on) and program the individual address. A router has no group
 *   objects. Do not hold BOOT0 while powering up: that enters the upload mode.
 *
 * Expect, on the Serial Monitor (on the board's USB port):
 *   "KNX IP router, address 1.1.1, not configured" before ETS, the address
 *   ETS gave it afterwards.
 */

OPENPLC_APP_VERSION(1, 0, 0);
OPENPLC_KNX_ROLE(IP_ROUTER);

#include <OpenPLC_KNX.h>

void printBanner()
{
  uint16_t ia = KNX.individualAddress();
  Serial.print("KNX IP router, address ");
  Serial.print(ia >> 12);
  Serial.print('.');
  Serial.print((ia >> 8) & 0x0F);
  Serial.print('.');
  Serial.print(ia & 0xFF);
  Serial.println(KNX.configured() ? ", configured" : ", not configured");
}

// Prints the description each time the Serial Monitor connects, not once in
// setup(): every reset re-enumerates USB and drops what setup() printed (README).
void printBannerOnConnect()
{
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

void setup()
{
  Serial.begin(115200);

  KNX.setup("OPENPLC-IPR");
  KNX.start();
}

void loop()
{
  printBannerOnConnect();
  KNX.loop();
}
