/*
 * KNX_Temperature -- receive a temperature from a KNX sensor and switch
 * another KNX device when it crosses a threshold.
 *
 * What it does: "Temperature" takes the temperature the sensor sends; the board
 * also asks for it at start-up and every 60 s, so it need not wait for the
 * sensor. Above the threshold "Temperature switch" sends 1 and DO2 gives 24 V; below the
 * threshold minus HYSTERESIS_C it sends 0 and DO2 goes off.
 * The threshold starts at 27.0 C after every power-up; type "t 28.5" on the
 * Serial Monitor to change it.
 *
 * Objects, by their name in ETS (product OpenPLC_TP.knxprod from the
 * OpenPLC_ETS_Prod repo; numbers in src/openplc_knx_objects.h):
 *   Temperature         DPT 9.001, written by the bus
 *   Temperature switch  DPT 1.001, sent and readable
 *
 * Board: the KNX TP bus on the KNX terminals.
 * ETS: import the product, press the BOOT0 button once while this sketch runs
 *   (the system LED stays on), program the individual address, download, and
 *   link "Temperature" to the sensor's temperature group address and
 *   "Temperature switch" to the switch group address of the device to drive.
 *   Steps in the OpenPLC_ETS_Prod README.
 *   Do not hold BOOT0 while powering up: that enters the upload mode.
 *
 * Expect, on the Serial Monitor:
 *   "KNX: address 1.1.20, configured" (or "not configured" before ETS), then
 *   "KNX RX temperature = 23.4" for each value, and "KNX TX switch = 1" and
 *   "DO2 = 1" when it rises above the threshold, "= 0" when it falls back;
 *   "threshold = 28.5" after "t 28.5".
 * Connect: a load or a multimeter on DO2.
 * Serial Monitor: on the board's USB port, line ending "Newline".
 */

OPENPLC_APP_VERSION(1, 0, 0);
OPENPLC_KNX_ROLE(TP_DEVICE);

#include <OpenPLC_KNX.h>

const float HYSTERESIS_C = 0.5;
float thresholdC = 27.0;
float lastC = 0;
const uint32_t readEveryMs = 60000;
bool switchedOn = false;
bool haveValue = false;
bool askedOnce = false;
uint32_t lastRead = 0;
String line;

GroupObject &temperatureObject() { return KNX.getGroupObject(KNX_OBJ_TEMPERATURE); }
GroupObject &switchObject() { return KNX.getGroupObject(KNX_OBJ_TEMPERATURE_SWITCH); }

void applyThreshold(float t)
{
  bool on = switchedOn;
  if (t > thresholdC) {
    on = true;
  } else if (t < thresholdC - HYSTERESIS_C) {
    on = false;
  }
  if (haveValue && on == switchedOn) {
    return;
  }
  haveValue = true;
  switchedOn = on;
  switchObject().value(on);
  digitalWrite(DOUT_2, on ? HIGH : LOW);
  Serial.print("KNX TX switch = ");
  Serial.println(on);
  Serial.print("DO2 = ");
  Serial.println(on);
}

void onTemperature(GroupObject &go)
{
  float t = go.value(DPT_Value_Temp);
  Serial.print("KNX RX temperature = ");
  Serial.println(t, 1);
  lastC = t;
  applyThreshold(t);
}

// "t 28.5" sets the threshold and applies it to the last temperature at once.
void readCommand()
{
  while (Serial.available()) {
    char c = Serial.read();
    if (c != '\n' && c != '\r') {
      line += c;
      continue;
    }
    line.trim();
    if (line.startsWith("t ")) {
      thresholdC = line.substring(2).toFloat();
      Serial.print("threshold = ");
      Serial.println(thresholdC, 1);
      if (haveValue) {
        applyThreshold(lastC);
      }
    } else if (line.length() > 0) {
      Serial.println("unknown command, use: t 28.5");
    }
    line = "";
  }
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
  pinMode(DOUT_2, OUTPUT);
  digitalWrite(DOUT_2, LOW);

  // The order number is the one in the ETS product.
  KNX.setup(OPENPLC_KNX_ORDER_NUMBER);
  if (KNX.configured()) {
    temperatureObject().dataPointType(DPT_Value_Temp);
    temperatureObject().callback(onTemperature);
    switchObject().dataPointType(DPT_Switch);
    switchObject().valueNoSend(false);
  }
  KNX.start();
}

void loop()
{
  printBannerOnConnect();
  KNX.loop();
  readCommand();
  if (!KNX.configured()) {
    return;
  }
  if (!askedOnce || millis() - lastRead >= readEveryMs) {
    askedOnce = true;
    lastRead = millis();
    temperatureObject().requestObjectRead();
  }
}
