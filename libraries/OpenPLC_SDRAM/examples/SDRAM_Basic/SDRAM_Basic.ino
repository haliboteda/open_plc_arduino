/*
 * SDRAM_Basic -- the smallest useful thing.
 *
 * Claims 4 MB of the board's external SDRAM, writes a pattern, reads it back.
 *
 * Wiring: none. Output goes to the RS232 terminals C05/C06 (115200 8N1).
 * ⚠️ Those are real RS-232 levels (+/-12 V). A 3.3 V USB-TTL adapter connected
 * there can be damaged -- use an RS-232 adapter.
 */

#include <OpenPLC_SDRAM.h>

const size_t BUFFER_BYTES = 4UL * 1024UL * 1024UL;   // 4 MB
uint32_t *buffer = nullptr;

void setup() {
  // The RS232 transceiver is off after reset; the sketch has to turn it on or
  // nothing reaches the terminals.
  pinMode(RS232_EN_Pin, OUTPUT);
  digitalWrite(RS232_EN_Pin, HIGH);
  Serial_Test.begin(115200);
  delay(200);

  // begin() powers up the SDRAM chip and checks it answers. It returns false
  // on a board where the memory is not usable -- decide here what that means
  // for your application. Carrying on and hoping is the one bad option.
  if (!SDRAM.begin()) {
    Serial_Test.println("SDRAM not available - stopping");
    while (true) { delay(1000); }
  }

  Serial_Test.print("SDRAM ready, ");
  Serial_Test.print(SDRAM.available() / (1024UL * 1024UL));
  Serial_Test.println(" MB free");

  // alloc() returns memory that is already zeroed. There is no free(): ask for
  // what you need once, in setup(), and keep it.
  buffer = (uint32_t *)SDRAM.alloc(BUFFER_BYTES);
  if (buffer == nullptr) {
    Serial_Test.println("allocation failed - stopping");
    while (true) { delay(1000); }
  }

  const size_t words = BUFFER_BYTES / sizeof(uint32_t);

  // Proof that it really is zeroed, before we write anything.
  Serial_Test.print("first word before writing: ");
  Serial_Test.println(buffer[0]);

  for (size_t i = 0; i < words; i++) {
    buffer[i] = (uint32_t)i * 2654435761UL;
  }

  size_t bad = 0;
  for (size_t i = 0; i < words; i++) {
    if (buffer[i] != (uint32_t)i * 2654435761UL) { bad++; }
  }

  Serial_Test.print("wrote and verified ");
  Serial_Test.print(words);
  Serial_Test.print(" words, mismatches: ");
  Serial_Test.println(bad);
  Serial_Test.print("still free: ");
  Serial_Test.print(SDRAM.available() / (1024UL * 1024UL));
  Serial_Test.println(" MB");
}

void loop() {
  delay(1000);
}
