/*
 * SD_FileReceive -- a file from the PC, over USB, onto the microSD card.
 *
 * What it does: waits for a file sent with YMODEM on the board's USB port,
 * writes it to the card under the name it was sent with, then reads it back
 * and prints its CRC-32. Reports when a card is inserted or removed.
 * Connect: a FAT32- or exFAT-formatted microSD card in the slot. Send from
 *   the board's USB port with Tera Term (File > Transfer > YMODEM > Send) or
 *   lrzsz's "sb"; close the Serial Monitor on that port first.
 * Expect: "SD: card inserted"; while a card is in, the board sends a "C" on
 *   USB once a second, which is YMODEM asking for a file. After a send
 *   "SD: wrote NAME, 12345 bytes, crc32=1A2B3C4D". The CRC-32 matches the one
 *   of the file on the PC (for example "crc32 FILE" on Linux).
 * Serial Monitor: on the board's USB port -- the same port YMODEM uses, so
 *   the result line is printed only after the transfer has ended.
 */

#include <OpenPLC_SD.h>
#include <OpenPLC_Ports.h>

OPENPLC_APP_VERSION(1, 1, 0);

const uint32_t debounceMs = 50;
bool cardIn = false;
bool cardReadable = false;
bool lastSample = false;
uint32_t lastSampleMs = 0;

File file;
char fileName[OPENPLC_YMODEM_NAME_MAX];
uint32_t fileSize = 0;
char report[128] = "";   // held back until YMODEM has let go of the port

int usbRead(void *, uint32_t timeoutMs)
{
  uint32_t start = millis();
  while (!Serial.available()) {
    if (millis() - start >= timeoutMs) {
      return -1;
    }
  }
  return Serial.read();
}

void usbWrite(void *, uint8_t b)
{
  Serial.write(b);
}

bool openFile(void *, const char *name, uint32_t size)
{
  strncpy(fileName, name, sizeof(fileName) - 1);
  fileSize = size;
  SD.remove(fileName);
  file = SD.open(fileName, FILE_WRITE);
  return (bool)file;
}

bool writeData(void *, const uint8_t *buf, uint32_t len)
{
  return file.write(buf, len) == len;
}

uint32_t crc32Update(uint32_t crc, const uint8_t *p, size_t n)
{
  while (n--) {
    crc ^= *p++;
    for (int i = 0; i < 8; i++) {
      crc = (crc & 1) ? (crc >> 1) ^ 0xEDB88320UL : crc >> 1;
    }
  }
  return crc;
}

void closeFile(void *, bool ok)
{
  file.close();
  if (!ok) {
    snprintf(report, sizeof(report), "SD: transfer of %s did not complete", fileName);
    return;
  }
  // Read back what is on the card, not what was received.
  File f = SD.open(fileName);
  uint32_t crc = 0xFFFFFFFFUL;
  uint32_t total = 0;
  uint8_t buf[256];
  int n;
  while ((n = f.read(buf, sizeof(buf))) > 0) {
    crc = crc32Update(crc, buf, n);
    total += n;
  }
  f.close();
  snprintf(report, sizeof(report), "SD: wrote %s, %lu bytes, crc32=%08lX",
           fileName, (unsigned long)total, (unsigned long)(crc ^ 0xFFFFFFFFUL));
}

const openplc_ymodem_io_t io = {nullptr, usbRead, usbWrite, openFile, writeData, closeFile};

void printCardInserted()
{
  Serial.println(cardReadable ? "SD: card inserted" : "SD: card inserted, but it could not be read");
}

void watchCard()
{
  if (millis() - lastSampleMs < debounceMs) {
    return;
  }
  lastSampleMs = millis();
  bool now = SD.cardPresent();
  if (now != lastSample) {
    lastSample = now;   // must read the same twice in a row
    return;
  }
  if (now == cardIn) {
    return;
  }
  cardIn = now;
  if (cardIn) {
    cardReadable = SD.begin();
    printCardInserted();
  } else {
    SD.end();
    Serial.println("SD: card removed");
  }
}

void printBanner()
{
  Serial.println("SD_FileReceive: send a file with YMODEM on USB.");
  if (cardIn) {
    printCardInserted();  // a card inserted before the monitor connected
  }
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
}

void loop()
{
  printBannerOnConnect();
  watchCard();
  if (cardIn) {
    // Asks the PC for a file for one second, then comes back to watch the card.
    openplcYmodemReceive(&io, 1000);
    if (report[0] != '\0') {
      Serial.println(report);
      report[0] = '\0';
    }
  }
}
