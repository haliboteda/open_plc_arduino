/*
 * First install "STM32duino STM32SD" from the Library Manager (it brings FatFs).
 *
 * SD_ReadWrite -- the microSD card slot.
 *
 * What it does: writes a line to TEST.TXT on the card, reads the file back and
 * prints it, once at start-up.
 *
 * Wiring: a FAT32- or exFAT-formatted microSD card in the slot.
 * Expect: "Read back: hello from OpenPLC" followed by "SD_ReadWrite: OK".
 * With no card the Serial Monitor says so.
 *
 * Serial Monitor: the board's USB port, 115200 baud.
 */

#include <STM32SD.h>

OPENPLC_APP_VERSION(1, 0, 0);

const char *fileName = "TEST.TXT";
const char *text = "hello from OpenPLC";

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}

  if (!SD.begin(SDMMC_CD_Pin)) {
    Serial.println("SD_ReadWrite: no card, or the card could not be read.");
    return;
  }

  SD.remove(fileName);
  File f = SD.open(fileName, FILE_WRITE);
  if (!f) {
    Serial.println("SD_ReadWrite: could not create the file.");
    return;
  }
  f.println(text);
  f.close();

  f = SD.open(fileName);
  if (!f) {
    Serial.println("SD_ReadWrite: could not open the file again.");
    return;
  }
  String line = f.readStringUntil('\n');
  f.close();
  line.trim();

  Serial.print("Read back: ");
  Serial.println(line);
  Serial.println(line == text ? "SD_ReadWrite: OK" : "SD_ReadWrite: MISMATCH");
}

void loop() {
}
