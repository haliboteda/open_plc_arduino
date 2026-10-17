/*
 * SDRAM_DataLogger -- what the 64 MB is actually for.
 *
 * Records an analog input into a ring buffer big enough to be useless in
 * internal RAM: 8 million samples at 2 bytes each is 16 MB, against the 512 KB
 * of SRAM the whole application otherwise shares.
 *
 * At 1 kHz that is 2 hours 13 minutes of history kept in memory, which is the
 * kind of window that makes "what happened just before the fault" answerable.
 *
 * Type 'd' into the Serial Monitor (on the board's USB port) to dump statistics.
 *
 * ⚠️ SDRAM contents do NOT survive a power cut. This is a live history buffer,
 * not storage. Anything that has to outlive a power-down belongs in flash or
 * on the SD card.
 */

// Every sketch declares its own version. The upload tool compares it with
// the one on the board and refuses to flash an older one over a newer one.
OPENPLC_APP_VERSION(1, 0, 0);

#include <OpenPLC_SDRAM.h>

const size_t SAMPLE_COUNT = 8UL * 1000UL * 1000UL;   // 16 MB as uint16_t
const uint32_t SAMPLE_INTERVAL_US = 1000;            // 1 kHz

uint16_t *history = nullptr;
size_t writeIndex = 0;
uint32_t totalSamples = 0;
uint32_t lastSampleUs = 0;
bool wrapped = false;
uint32_t allocMs = 0;
const char *failure = nullptr;  // why setup() stopped, or nullptr

void printBanner() {
  if (failure != nullptr) {
    Serial.println(failure);
    return;
  }
  Serial.println();
  Serial.println("=== SDRAM data logger ===");
  Serial.print("history: ");
  Serial.print(SAMPLE_COUNT);
  Serial.print(" samples (");
  Serial.print((SAMPLE_COUNT * sizeof(uint16_t)) / (1024UL * 1024UL));
  Serial.print(" MB), allocated and cleared in ");
  Serial.print(allocMs);
  Serial.println(" ms");

  Serial.print("that is ");
  Serial.print((SAMPLE_COUNT / 1000UL) / 60UL);
  Serial.println(" minutes of history at 1 kHz");
  Serial.println("press 'd' for a dump");
}

// Prints the description each time the Serial Monitor connects, not once in
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

  if (!SDRAM.begin()) {
    // Without the buffer this sketch has nothing to do. A real application
    // would more likely fall back to a much shorter in-SRAM history and carry
    // on controlling the plant -- losing the log is not a reason to stop.
    failure = "SDRAM unavailable - logging disabled";
    while (true) { printBannerOnConnect(); }
  }

  // 16 MB of zeroing costs roughly 180 ms. That is fine here because it
  // happens once in setup(); it would not be fine inside loop().
  uint32_t t0 = millis();
  history = (uint16_t *)SDRAM.alloc(SAMPLE_COUNT * sizeof(uint16_t));
  allocMs = millis() - t0;

  if (history == nullptr) {
    failure = "could not allocate the history buffer - stopping";
    while (true) { printBannerOnConnect(); }
  }

  lastSampleUs = micros();
}

void loop() {
  printBannerOnConnect();
  // Sample on a fixed interval. micros() subtraction is wrap-safe.
  if ((uint32_t)(micros() - lastSampleUs) >= SAMPLE_INTERVAL_US) {
    lastSampleUs += SAMPLE_INTERVAL_US;

    history[writeIndex] = (uint16_t)analogRead(AIN_1);
    totalSamples++;

    if (++writeIndex >= SAMPLE_COUNT) {
      writeIndex = 0;
      wrapped = true;      // the ring is full; oldest data now gets overwritten
    }
  }

  if (Serial.available() && Serial.read() == 'd') {
    dumpStats();
  }
}

void dumpStats() {
  size_t have = wrapped ? SAMPLE_COUNT : writeIndex;
  if (have == 0) {
    Serial.println("no samples yet");
    return;
  }

  // Walk the whole ring. 16 M reads is slow enough to notice -- a control loop
  // with real work in it should do this in slices, not in one go.
  uint32_t sum = 0;
  uint16_t lo = 0xFFFF, hi = 0;
  for (size_t i = 0; i < have; i++) {
    uint16_t v = history[i];
    sum += v;
    if (v < lo) { lo = v; }
    if (v > hi) { hi = v; }
  }

  Serial.print("samples held: ");
  Serial.print(have);
  Serial.print(wrapped ? " (ring full, oldest overwritten)" : " (still filling)");
  Serial.println();
  Serial.print("total taken:  ");
  Serial.println(totalSamples);
  Serial.print("min/mean/max: ");
  Serial.print(lo);
  Serial.print(" / ");
  Serial.print(sum / have);
  Serial.print(" / ");
  Serial.println(hi);
}
