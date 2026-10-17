/*
 * SDRAM_Basic -- the smallest useful thing.
 *
 * Claims 4 MB of the board's external SDRAM, writes a pattern, reads it back,
 * once at start-up; prints the result each time the Serial Monitor connects.
 *
 * Wiring: none.
 * Serial Monitor: on the board's USB port.
 */

// Every sketch declares its own version. The upload tool compares it with
// the one on the board and refuses to flash an older one over a newer one.
OPENPLC_APP_VERSION(1, 0, 0);

#include <OpenPLC_SDRAM.h>

const size_t BUFFER_BYTES = 4UL * 1024UL * 1024UL;   // 4 MB
const size_t WORDS = BUFFER_BYTES / sizeof(uint32_t);
uint32_t *buffer = nullptr;

// What setup() found, kept so it can be printed again.
const char *failure = nullptr;  // why it stopped, or nullptr
bool ready = false;
size_t readyMb = 0;
bool verified = false;
uint32_t firstWord = 0;
size_t bad = 0;
size_t stillFreeMb = 0;

void printBanner() {
  if (ready) {
    Serial.print("SDRAM ready, ");
    Serial.print(readyMb);
    Serial.println(" MB free");
  }
  if (failure != nullptr) {
    Serial.println(failure);
    return;
  }
  if (!verified) {
    return;
  }
  Serial.print("first word before writing: ");
  Serial.println(firstWord);
  Serial.print("wrote and verified ");
  Serial.print(WORDS);
  Serial.print(" words, mismatches: ");
  Serial.println(bad);
  Serial.print("still free: ");
  Serial.print(stillFreeMb);
  Serial.println(" MB");
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
  delay(200);

  // begin() powers up the SDRAM chip and checks it answers. It returns false
  // on a board where the memory is not usable -- decide here what that means
  // for your application. Carrying on and hoping is the one bad option.
  if (!SDRAM.begin()) {
    failure = "SDRAM not available - stopping";
    while (true) { printBannerOnConnect(); }
  }

  ready = true;
  readyMb = SDRAM.available() / (1024UL * 1024UL);

  // alloc() returns memory that is already zeroed. There is no free(): ask for
  // what you need once, in setup(), and keep it.
  buffer = (uint32_t *)SDRAM.alloc(BUFFER_BYTES);
  if (buffer == nullptr) {
    failure = "allocation failed - stopping";
    while (true) { printBannerOnConnect(); }
  }

  // Proof that it really is zeroed, before we write anything.
  firstWord = buffer[0];

  for (size_t i = 0; i < WORDS; i++) {
    buffer[i] = (uint32_t)i * 2654435761UL;
  }

  for (size_t i = 0; i < WORDS; i++) {
    if (buffer[i] != (uint32_t)i * 2654435761UL) { bad++; }
  }

  stillFreeMb = SDRAM.available() / (1024UL * 1024UL);
  verified = true;
}

void loop() {
  printBannerOnConnect();
}
