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
 * Type 'd' into the terminal (115200 8N1 on C05/C06) to dump statistics.
 *
 * ⚠️ SDRAM contents do NOT survive a power cut. This is a live history buffer,
 * not storage. Anything that has to outlive a power-down belongs in flash or
 * on the SD card.
 */

#include <OpenPLC_SDRAM.h>

const size_t SAMPLE_COUNT = 8UL * 1000UL * 1000UL;   // 16 MB as uint16_t
const uint32_t SAMPLE_INTERVAL_US = 1000;            // 1 kHz

uint16_t *history = nullptr;
size_t writeIndex = 0;
uint32_t totalSamples = 0;
uint32_t lastSampleUs = 0;
bool wrapped = false;

void setup() {
  pinMode(RS232_EN_Pin, OUTPUT);
  digitalWrite(RS232_EN_Pin, HIGH);
  Serial_Test.begin(115200);
  delay(200);
  Serial_Test.println();
  Serial_Test.println("=== SDRAM data logger ===");

  if (!SDRAM.begin()) {
    // Without the buffer this sketch has nothing to do. A real application
    // would more likely fall back to a much shorter in-SRAM history and carry
    // on controlling the plant -- losing the log is not a reason to stop.
    Serial_Test.println("SDRAM unavailable - logging disabled");
    while (true) { delay(1000); }
  }

  // 16 MB of zeroing costs roughly 180 ms. That is fine here because it
  // happens once in setup(); it would not be fine inside loop().
  uint32_t t0 = millis();
  history = (uint16_t *)SDRAM.alloc(SAMPLE_COUNT * sizeof(uint16_t));
  uint32_t allocMs = millis() - t0;

  if (history == nullptr) {
    Serial_Test.println("could not allocate the history buffer - stopping");
    while (true) { delay(1000); }
  }

  Serial_Test.print("history: ");
  Serial_Test.print(SAMPLE_COUNT);
  Serial_Test.print(" samples (");
  Serial_Test.print((SAMPLE_COUNT * sizeof(uint16_t)) / (1024UL * 1024UL));
  Serial_Test.print(" MB), allocated and cleared in ");
  Serial_Test.print(allocMs);
  Serial_Test.println(" ms");

  Serial_Test.print("that is ");
  Serial_Test.print((SAMPLE_COUNT / 1000UL) / 60UL);
  Serial_Test.println(" minutes of history at 1 kHz");
  Serial_Test.println("press 'd' for a dump");

  lastSampleUs = micros();
}

void loop() {
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

  if (Serial_Test.available() && Serial_Test.read() == 'd') {
    dumpStats();
  }
}

void dumpStats() {
  size_t have = wrapped ? SAMPLE_COUNT : writeIndex;
  if (have == 0) {
    Serial_Test.println("no samples yet");
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

  Serial_Test.print("samples held: ");
  Serial_Test.print(have);
  Serial_Test.print(wrapped ? " (ring full, oldest overwritten)" : " (still filling)");
  Serial_Test.println();
  Serial_Test.print("total taken:  ");
  Serial_Test.println(totalSamples);
  Serial_Test.print("min/mean/max: ");
  Serial_Test.print(lo);
  Serial_Test.print(" / ");
  Serial_Test.print(sum / have);
  Serial_Test.print(" / ");
  Serial_Test.println(hi);
}
