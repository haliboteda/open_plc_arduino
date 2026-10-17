/*
 * KNX_BusMonitor -- print every telegram on the KNX TP bus and send group
 * writes onto it. No ETS configuration needed.
 *
 * Board: the KNX TP bus,
 *   with its power supply, on the KNX terminals.
 *
 * Serial Monitor: on the board's USB port, line ending "Newline".
 *   w 1/0/1 1         GroupValue.Write of a 0..63 value (switch on = 1)
 *   b 1/0/1 0C 1A     GroupValue.Write of data octets, in hex (up to 14)
 *
 * Expect:
 *   "KNX RX 1.1.20 -> 1/0/1 Write 1" for each telegram another device sends,
 *   "KNX TX 1.1.1 -> 1/0/1 Write 1" (1.1.1 until ETS gives the board an
 *   address) then "KNX TX acknowledged" (or "not acknowledged" when no device
 *   on the bus listens to that group).
 */

OPENPLC_APP_VERSION(1, 0, 0);
OPENPLC_KNX_ROLE(TP_DEVICE);

#include <OpenPLC_KNX.h>

String line;

void printIndividual(uint16_t a)
{
  Serial.print(a >> 12);
  Serial.print('.');
  Serial.print((a >> 8) & 0x0F);
  Serial.print('.');
  Serial.print(a & 0xFF);
}

void printGroup(uint16_t a)
{
  Serial.print(a >> 11);
  Serial.print('/');
  Serial.print((a >> 8) & 0x07);
  Serial.print('/');
  Serial.print(a & 0xFF);
}

void printHex(uint8_t b)
{
  if (b < 0x10) {
    Serial.print('0');
  }
  Serial.print(b, HEX);
}

void onFrame(const KnxBusFrame &f)
{
  Serial.print("KNX RX ");
  printIndividual(f.source);
  Serial.print(" -> ");
  if (f.group) {
    printGroup(f.destination);
  } else {
    printIndividual(f.destination);
  }
  if (f.tpduLen < 2) {
    Serial.println(" (no data)");
    return;
  }
  uint8_t apci = ((f.tpdu[0] & 0x03) << 2) | (f.tpdu[1] >> 6);
  if (apci == 0) {
    Serial.print(" Read");
  } else if (apci == 1) {
    Serial.print(" Response");
  } else if (apci == 2) {
    Serial.print(" Write");
  } else {
    Serial.print(" APCI ");
    Serial.print(apci);
  }
  if (apci == 1 || apci == 2) {
    if (f.tpduLen == 2) {
      Serial.print(' ');
      Serial.print(f.tpdu[1] & 0x3F);
    } else {
      for (uint8_t i = 2; i < f.tpduLen; i++) {
        Serial.print(' ');
        printHex(f.tpdu[i]);
      }
    }
  }
  Serial.println();
}

void onSent(bool ok)
{
  Serial.println(ok ? "KNX TX acknowledged" : "KNX TX not acknowledged");
}

// "1/0/1" -> group address; false when it is not one.
bool parseGroup(const String &s, uint16_t &ga)
{
  int a = s.indexOf('/');
  int b = s.indexOf('/', a + 1);
  if (a < 1 || b < a + 2) {
    return false;
  }
  long main = s.substring(0, a).toInt();
  long middle = s.substring(a + 1, b).toInt();
  long sub = s.substring(b + 1).toInt();
  if (main > 31 || middle > 7 || sub > 255) {
    return false;
  }
  ga = (uint16_t)((main << 11) | (middle << 8) | sub);
  return true;
}

void command(String s)
{
  s.trim();
  int sp = s.indexOf(' ');
  int sp2 = s.indexOf(' ', sp + 1);
  uint16_t ga;
  if (sp != 1 || sp2 < 0 || !parseGroup(s.substring(2, sp2), ga)) {
    Serial.println("usage: w 1/0/1 1  |  b 1/0/1 0C 1A");
    return;
  }
  String rest = s.substring(sp2 + 1);
  bool queued;
  if (s[0] == 'w') {
    long v = rest.toInt();
    queued = (v >= 0 && v <= 63) && KNX.sendGroupWrite(ga, (uint8_t)v);
  } else if (s[0] == 'b') {
    uint8_t data[14];
    uint8_t n = 0;
    rest.trim();
    while (rest.length() > 0 && n < sizeof(data)) {
      int e = rest.indexOf(' ');
      String tok = (e < 0) ? rest : rest.substring(0, e);
      data[n++] = (uint8_t)strtol(tok.c_str(), nullptr, 16);
      rest = (e < 0) ? String() : rest.substring(e + 1);
      rest.trim();
    }
    queued = (n > 0) && KNX.sendGroupWrite(ga, data, n);
  } else {
    Serial.println("usage: w 1/0/1 1  |  b 1/0/1 0C 1A");
    return;
  }
  if (!queued) {
    Serial.println("KNX TX refused (bad value, or the last one is still going out)");
    return;
  }
  Serial.print("KNX TX ");
  printIndividual(KNX.individualAddress());
  Serial.print(" -> ");
  printGroup(ga);
  Serial.print(" Write ");
  Serial.println(s.substring(sp2 + 1));
}

void printBanner()
{
  Serial.print("KNX bus monitor, this board is ");
  printIndividual(KNX.individualAddress());
  Serial.println();
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

  KNX.setup(OPENPLC_KNX_ORDER_NUMBER);
  KNX.onBusFrame(onFrame);
  KNX.onBusSendDone(onSent);
  KNX.start();
}

void loop()
{
  printBannerOnConnect();
  KNX.loop();
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (line.length()) {
        command(line);
      }
      line = "";
    } else if (line.length() < 80) {
      line += c;
    }
  }
}
