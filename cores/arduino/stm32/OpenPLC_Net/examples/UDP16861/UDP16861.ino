#include <OpenPLC_Net.h>

void setup() {
  OpenPLCNet.begin();
}

void loop() {
  OpenPLCNet.process();
}
