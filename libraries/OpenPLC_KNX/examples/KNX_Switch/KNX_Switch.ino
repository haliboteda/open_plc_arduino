/*
 * KNX_Switch -- the six relays as a KNX switch actuator.
 *
 * What it does: each relay has a switch object the bus writes and a status
 * object the board sends back once the relay has actually changed. The
 * status objects also answer read requests.
 *
 * Objects, by their name in ETS (product OpenPLC_TP.knxprod from the
 * OpenPLC_ETS_Prod repo; numbers in src/openplc_knx_objects.h):
 *   Relay 1..6 switch   DPT 1.001, written by the bus
 *   Relay 1..6 status   DPT 1.001, sent and readable
 *
 * Board: the KNX TP bus on the KNX terminals.
 * ETS: import the product, press the BOOT0 button once while this sketch runs
 *   (the system LED stays on), program the individual address, download, and
 *   link group addresses to the objects. Steps in the OpenPLC_ETS_Prod README.
 *   Do not hold BOOT0 while powering up: that enters the upload mode.
 *
 * Expect, on the Serial Monitor:
 *   "KNX: address 1.1.20, configured" (or "not configured" before ETS), then
 *   for a write from the bus "KNX RX relay 3 switch = 1", the relay clicks,
 *   and "KNX TX relay 3 status = 1".
 * Serial Monitor: on the board's USB port.
 */

OPENPLC_APP_VERSION(1, 0, 0);
OPENPLC_KNX_ROLE(TP_DEVICE);

#include <OpenPLC_KNX.h>

const int relays[] = {REL_1, REL_2, REL_3, REL_4, REL_5, REL_6};
const int count = sizeof(relays) / sizeof(relays[0]);
bool state[count];

const uint16_t switchObjects[count] = {KNX_OBJ_RELAY1_SWITCH, KNX_OBJ_RELAY2_SWITCH, KNX_OBJ_RELAY3_SWITCH,
                                       KNX_OBJ_RELAY4_SWITCH, KNX_OBJ_RELAY5_SWITCH, KNX_OBJ_RELAY6_SWITCH};
const uint16_t statusObjects[count] = {KNX_OBJ_RELAY1_STATUS, KNX_OBJ_RELAY2_STATUS, KNX_OBJ_RELAY3_STATUS,
                                       KNX_OBJ_RELAY4_STATUS, KNX_OBJ_RELAY5_STATUS, KNX_OBJ_RELAY6_STATUS};

GroupObject &switchObject(int i) { return KNX.getGroupObject(switchObjects[i]); }
GroupObject &statusObject(int i) { return KNX.getGroupObject(statusObjects[i]); }

void onSwitch(GroupObject &go)
{
  int i = 0;
  while (i < count && switchObjects[i] != go.asap()) {
    i++;
  }
  if (i == count) {
    return;
  }
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

void printBanner()
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
  for (int i = 0; i < count; i++) {
    pinMode(relays[i], OUTPUT);
    digitalWrite(relays[i], LOW);
    state[i] = false;
  }

  // The order number is the one in the ETS product.
  KNX.setup(OPENPLC_KNX_ORDER_NUMBER);
  if (KNX.configured()) {
    for (int i = 0; i < count; i++) {
      switchObject(i).dataPointType(DPT_Switch);
      switchObject(i).callback(onSwitch);
      statusObject(i).dataPointType(DPT_Switch);
      statusObject(i).valueNoSend(false);
    }
  }
  KNX.start();
}

void loop()
{
  printBannerOnConnect();
  KNX.loop();
}
