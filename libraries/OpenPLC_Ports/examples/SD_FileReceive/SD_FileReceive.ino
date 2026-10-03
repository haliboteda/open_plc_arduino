/*
 * First install "STM32duino STM32SD" from the Library Manager (it brings FatFs).
 *
 * SD_FileReceive -- a file from the PC, over RS232, onto the microSD card.
 *
 * What it does: waits for a file sent with YMODEM on the RS232 port, writes it
 * to the card under the name it was sent with, then reads it back and prints
 * its CRC-32. Reports when a card is inserted or removed.
 * Connect: a FAT32- or exFAT-formatted microSD card in the slot, and a PC
 *   through an RS232 adapter (real RS232 levels, not a 3.3 V USB-TTL adapter),
 *   115200 8N1. Send with Tera Term (File > Transfer > YMODEM > Send) or
 *   lrzsz's "sb". The board prints one "[NET]" line on RS232 when it gets an
 *   address; the sender retries past it, but with no Ethernet cable there is
 *   nothing to retry.
 * Expect: "SD: card inserted"; while a card is in, the board sends a "C" on
 *   RS232 once a second, which is YMODEM asking for a file. After a send
 *   "SD: wrote NAME, 12345 bytes, crc32=1A2B3C4D". The CRC-32 matches the one
 *   of the file on the PC (for example "crc32 FILE" on Linux).
 * Serial Monitor: the board's USB port, 115200 baud.
 */

#include <STM32SD.h>
#include <OpenPLC_Ports.h>

OPENPLC_APP_VERSION(1, 0, 0);

const uint32_t debounceMs = 50;
bool cardIn = false;
bool lastSample = false;
uint32_t lastSampleMs = 0;

File file;
char fileName[OPENPLC_YMODEM_NAME_MAX];
uint32_t fileSize = 0;

int rs232Read(void *, uint32_t timeoutMs)
{
  uint32_t start = millis();
  while (!Serial_Test.available()) {
    if (millis() - start >= timeoutMs) {
      return -1;
    }
  }
  return Serial_Test.read();
}

void rs232Write(void *, uint8_t b)
{
  Serial_Test.write(b);
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
    Serial.print("SD: transfer of ");
    Serial.print(fileName);
    Serial.println(" did not complete");
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
  Serial.print("SD: wrote ");
  Serial.print(fileName);
  Serial.print(", ");
  Serial.print(total);
  Serial.print(" bytes, crc32=");
  char hex[9];
  snprintf(hex, sizeof(hex), "%08lX", (unsigned long)(crc ^ 0xFFFFFFFFUL));
  Serial.println(hex);
}

const openplc_ymodem_io_t io = {nullptr, rs232Read, rs232Write, openFile, writeData, closeFile};

// The card-detect switch pulls SDMMC_CD_Pin low while a card is in.
void watchCard()
{
  if (millis() - lastSampleMs < debounceMs) {
    return;
  }
  lastSampleMs = millis();
  bool now = digitalRead(SDMMC_CD_Pin) == LOW;
  if (now != lastSample) {
    lastSample = now;   // must read the same twice in a row
    return;
  }
  if (now == cardIn) {
    return;
  }
  cardIn = now;
  if (cardIn) {
    Serial.println(SD.begin(SDMMC_CD_Pin) ? "SD: card inserted" : "SD: card inserted, but it could not be read");
  } else {
    SD.end();
    Serial.println("SD: card removed");
  }
}

void setup()
{
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}
  pinMode(RS232_EN_Pin, OUTPUT);
  digitalWrite(RS232_EN_Pin, HIGH);  // the transceiver is off until this pin is high
  Serial_Test.begin(115200);
  pinMode(SDMMC_CD_Pin, INPUT);      // the board has its own pull-up
  Serial.println("SD_FileReceive: send a file with YMODEM on RS232.");
}

void loop()
{
  watchCard();
  if (cardIn) {
    // Asks the PC for a file for one second, then comes back to watch the card.
    openplcYmodemReceive(&io, 1000);
  }
}
