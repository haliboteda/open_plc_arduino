/*
 * KNX_Analog -- the analog inputs and outputs as KNX 2-byte float values.
 *
 * What it does: sends AI1 (mV) and AI2 (mA) once at start-up and again
 * whenever one moves by more than 100 mV / 0.2 mA from the value last sent;
 * both objects also answer read requests. A value written to "AO 1 setpoint" or
 * "AO 2 setpoint" sets AO1 or AO2 to that current. All values use this board's calibration.
 *
 * Objects, by their name in ETS (product OpenPLC_TP.knxprod from the
 * OpenPLC_ETS_Prod repo; numbers in src/openplc_knx_objects.h):
 *   AI 1 voltage    DPT 9.020 (mV), sent and readable
 *   AI 2 current    DPT 9.021 (mA), sent and readable
 *   AO 1 setpoint   DPT 9.021 (mA), written by the bus, 0..20 mA
 *   AO 2 setpoint   DPT 9.021 (mA), written by the bus, 0..20 mA
 *
 * Board: the KNX TP bus on the KNX terminals;
 *   a current meter on AO1 / AO2 to see the outputs. AI1 and AI2 need their
 *   solder jumpers closed, or they read about 0.
 * ETS: import the product, press the BOOT0 button once while this sketch runs
 *   (the system LED stays on), program the individual address, download, and
 *   link group addresses to the objects. Steps in the OpenPLC_ETS_Prod README.
 *   Do not hold BOOT0 while powering up: that enters the upload mode.
 *
 * Expect, on the Serial Monitor:
 *   "KNX: address 1.1.20, configured" (or "not configured" before ETS), then
 *   "KNX TX AI1 = 2500 mV" and "KNX TX AI2 = 4.00 mA" for each value sent, and
 *   "KNX RX AO1 = 10.00 mA" for each setpoint written; the meter on AO1 then
 *   reads about 10 mA.
 * Serial Monitor: on the board's USB port.
 */

OPENPLC_APP_VERSION(1, 0, 0);
OPENPLC_KNX_ROLE(TP_DEVICE);

#include <OpenPLC_KNX.h>
#include <OpenPLC_Ports.h>

const float AI1_STEP_MV = 100.0;
const float AI2_STEP_MA = 0.2;
const uint32_t sampleMs = 100;
bool sentOnce = false;
float sentAI1 = 0;
float sentAI2 = 0;
uint32_t lastSample = 0;

const uint16_t aoObjects[2] = {KNX_OBJ_AO1_SETPOINT, KNX_OBJ_AO2_SETPOINT};

GroupObject &ai1Object() { return KNX.getGroupObject(KNX_OBJ_AI1_VOLTAGE); }
GroupObject &ai2Object() { return KNX.getGroupObject(KNX_OBJ_AI2_CURRENT); }
GroupObject &aoObject(uint8_t channel) { return KNX.getGroupObject(aoObjects[channel - 1]); }

void onSetpoint(GroupObject &go)
{
  uint8_t channel = (go.asap() == KNX_OBJ_AO1_SETPOINT) ? 1 : 2;
  float mA = go.value(DPT_Value_Curr);
  openplcWriteAO_mA(channel, mA);
  Serial.print("KNX RX AO");
  Serial.print(channel);
  Serial.print(" = ");
  Serial.print(mA, 2);
  Serial.println(" mA");
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

  // The order number is the one in the ETS product.
  KNX.setup(OPENPLC_KNX_ORDER_NUMBER);
  if (KNX.configured()) {
    ai1Object().dataPointType(DPT_Value_Volt);
    ai2Object().dataPointType(DPT_Value_Curr);
    for (uint8_t channel = 1; channel <= 2; channel++) {
      aoObject(channel).dataPointType(DPT_Value_Curr);
      aoObject(channel).callback(onSetpoint);
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

  float mV = openplcReadAI1_mV();
  if (!sentOnce || fabsf(mV - sentAI1) > AI1_STEP_MV) {
    sentAI1 = mV;
    ai1Object().value(mV);
    Serial.print("KNX TX AI1 = ");
    Serial.print(mV, 0);
    Serial.println(" mV");
  }
  float mA = openplcReadAI2_mA();
  if (!sentOnce || fabsf(mA - sentAI2) > AI2_STEP_MA) {
    sentAI2 = mA;
    ai2Object().value(mA);
    Serial.print("KNX TX AI2 = ");
    Serial.print(mA, 2);
    Serial.println(" mA");
  }
  sentOnce = true;
}
