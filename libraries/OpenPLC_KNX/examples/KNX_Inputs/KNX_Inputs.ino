/*
 * KNX_Inputs -- the eight digital inputs as a KNX binary input.
 *
 * What it does: samples DI1..DI8 every 20 ms and sends an input's object
 * whenever it changes. The objects also answer read requests.
 *
 * Objects, by their name in ETS (product OpenPLC_TP.knxprod from the
 * OpenPLC_ETS_Prod repo; numbers in src/openplc_knx_objects.h):
 *   DI 1..8   DPT 1.001, sent and readable
 *
 * Board: the KNX TP bus on the KNX terminals;
 *   24 V supply to the board, and 24 V switched onto the input you test.
 * ETS: import the product, press the BOOT0 button once while this sketch runs
 *   (the system LED stays on), program the individual address, download, and
 *   link group addresses to the objects. Steps in the OpenPLC_ETS_Prod README.
 *   Do not hold BOOT0 while powering up: that enters the upload mode.
 *
 * Expect, on the Serial Monitor:
 *   "KNX: address 1.1.20, configured" (or "not configured" before ETS), then
 *   "KNX TX DI 2 = 1" when 24 V is applied to DI2, "= 0" when it is removed.
 * Serial Monitor: on the board's USB port.
 */

OPENPLC_APP_VERSION(1, 0, 0);
OPENPLC_KNX_ROLE(TP_DEVICE);

#include <OpenPLC_KNX.h>

const int inputs[] = {DIN_1, DIN_2, DIN_3, DIN_4, DIN_5, DIN_6, DIN_7, DIN_8};
const int count = sizeof(inputs) / sizeof(inputs[0]);
const uint16_t inputObjects[count] = {KNX_OBJ_DI1, KNX_OBJ_DI2, KNX_OBJ_DI3, KNX_OBJ_DI4,
                                      KNX_OBJ_DI5, KNX_OBJ_DI6, KNX_OBJ_DI7, KNX_OBJ_DI8};
const uint32_t sampleMs = 20;
bool state[count];
uint32_t lastSample = 0;

GroupObject &inputObject(int i) { return KNX.getGroupObject(inputObjects[i]); }

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
    pinMode(inputs[i], INPUT);  // the board has its own pull-ups; do not add INPUT_PULLUP
    state[i] = digitalRead(inputs[i]);
  }

  // The order number is the one in the ETS product.
  KNX.setup(OPENPLC_KNX_ORDER_NUMBER);
  if (KNX.configured()) {
    for (int i = 0; i < count; i++) {
      inputObject(i).dataPointType(DPT_Switch);
      inputObject(i).valueNoSend(state[i]);  // so a read is answered before the first change
    }
  }
  KNX.start();
}

void loop()
{
  printBannerOnConnect();
  KNX.loop();
  if (!KNX.configured() || millis() - lastSample < sampleMs) {
    return;
  }
  lastSample = millis();
  for (int i = 0; i < count; i++) {
    bool now = digitalRead(inputs[i]);
    if (now == state[i]) {
      continue;
    }
    state[i] = now;
    inputObject(i).value(now);
    Serial.print("KNX TX DI ");
    Serial.print(i + 1);
    Serial.print(" = ");
    Serial.println(now);
  }
}
