/*
 * KNX_Switch -- the six relays as a KNX switch actuator.
 *
 * What it does: each relay has a switch object the bus writes and a status
 * object the board sends back once the relay has actually changed. The
 * status objects also answer read requests.
 *
 * Objects (ETS product OpenPLC_TP.knxprod, from the OpenPLC_ETS_Prod repo):
 *   1, 3, 5, 7, 9, 11   relay 1..6 switch   DPT 1.001, written by the bus
 *   2, 4, 6, 8, 10, 12  relay 1..6 status   DPT 1.001, sent and readable
 *
 * Board: Tools > KNX Role > KNX TP Device. The KNX TP bus on the KNX terminals.
 * ETS: import the product, press the BOOT0 button once while this sketch runs
 *   (the system LED stays on), program the individual address, download, and
 *   link group addresses to the objects. Steps in the OpenPLC_ETS_Prod README.
 *   Do not hold BOOT0 while powering up: that enters the upload mode.
 *
 * Expect, on the Serial Monitor:
 *   "KNX: address 1.1.20, configured" (or "not configured" before ETS), then
 *   for a write from the bus "KNX RX relay 3 switch = 1", the relay clicks,
 *   and "KNX TX relay 3 status = 1".
 * Serial Monitor: the board's USB port, 115200 baud.
 */

OPENPLC_APP_VERSION(1, 0, 0);

#include <OpenPLC_KNX.h>

#if MASK_VERSION != 0x07B0
#error "KNX_Switch is a TP device: choose Tools > KNX Role > KNX TP Device"
#endif

const int relays[] = {REL_1, REL_2, REL_3, REL_4, REL_5, REL_6};
const int count = sizeof(relays) / sizeof(relays[0]);
bool state[count];

GroupObject &switchObject(int i) { return KNX.getGroupObject(2 * i + 1); }
GroupObject &statusObject(int i) { return KNX.getGroupObject(2 * i + 2); }

void onSwitch(GroupObject &go)
{
  int i = (go.asap() - 1) / 2;
  bool on = go.value();
  Serial.print("KNX RX relay ");
  Serial.print(i + 1);
  Serial.print(" switch = ");
  Serial.println(on);
  if (on == state[i]) {
    return;
  }
  digitalWrite(relays[i], on ? HIGH : LOW);
  state[i] = on;
  statusObject(i).value(on);
  Serial.print("KNX TX relay ");
  Serial.print(i + 1);
  Serial.print(" status = ");
  Serial.println(on);
}

void printAddress()
{
  uint16_t ia = KNX.individualAddress();
  if (!KNX.configured()) {
    Serial.println("KNX: not configured - program it with ETS");
    return;
  }
  Serial.print("KNX: address ");
  Serial.print(ia >> 12);
  Serial.print('.');
  Serial.print((ia >> 8) & 0x0F);
  Serial.print('.');
  Serial.print(ia & 0xFF);
  Serial.println(", configured");
}

void setup()
{
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}
  for (int i = 0; i < count; i++) {
    pinMode(relays[i], OUTPUT);
    digitalWrite(relays[i], LOW);
    state[i] = false;
  }

  // The order number is the one in the ETS product.
  KNXHelper.setup("OPENPLC-TP");
  printAddress();
  if (KNX.configured()) {
    for (int i = 0; i < count; i++) {
      switchObject(i).dataPointType(DPT_Switch);
      switchObject(i).callback(onSwitch);
      statusObject(i).dataPointType(DPT_Switch);
      statusObject(i).valueNoSend(false);
    }
  }
  KNXHelper.start();
}

void loop()
{
  KNXHelper.loop();
}
