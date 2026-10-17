/*
 * SD_ReadWrite -- the microSD card slot.
 *
 * What it does: writes a line to TEST.TXT on the card and reads the file back,
 * once at start-up; prints the result each time the Serial Monitor connects.
 *
 * Wiring: a FAT32- or exFAT-formatted microSD card in the slot.
 * Expect: "Read back: hello from OpenPLC" followed by "SD_ReadWrite: OK".
 * With no card the Serial Monitor says so.
 *
 * Serial Monitor: on the board's USB port.
 */

#include <OpenPLC_SD.h>

OPENPLC_APP_VERSION(1, 0, 0);

const char *fileName = "TEST.TXT";
const char *text = "hello from OpenPLC";

const char *failure = nullptr;  // what went wrong, or nullptr
String readBack;

void printBanner() {
  if (failure != nullptr) {
    Serial.println(failure);
    return;
  }
  Serial.print("Read back: ");
  Serial.println(readBack);
  Serial.println(readBack == text ? "SD_ReadWrite: OK" : "SD_ReadWrite: MISMATCH");
}

// Prints the result each time the Serial Monitor connects, not once in
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

  if (!SD.begin()) {
    failure = "SD_ReadWrite: no card, or the card could not be read.";
    return;
  }

  SD.remove(fileName);
  File f = SD.open(fileName, FILE_WRITE);
  if (!f) {
    failure = "SD_ReadWrite: could not create the file.";
    return;
  }
  f.println(text);
  f.close();

  f = SD.open(fileName);
  if (!f) {
    failure = "SD_ReadWrite: could not open the file again.";
    return;
  }
  readBack = f.readStringUntil('\n');
  f.close();
  readBack.trim();
}

void loop() {
  printBannerOnConnect();
}
