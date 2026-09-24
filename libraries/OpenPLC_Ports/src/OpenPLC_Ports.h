/*
 * OpenPLC_Ports -- helpers the port examples share.
 */

#ifndef OPENPLC_PORTS_H_
#define OPENPLC_PORTS_H_

#include <Arduino.h>

/* Full scale of analogRead()/analogWrite() once openplcEnableVref() has run. */
#define OPENPLC_VREF_MV 2500U

/* Turns on the chip's internal 2.5 V reference. The board has no external one,
 * so analogRead() and the DAC mean nothing without it. See
 * $PROD/docs/hardware/HARDWARE-FACTS.md, "模拟量：没有外部基准". Returns false if
 * the reference did not come up within 10 ms. */
bool openplcEnableVref(void);

#endif /* OPENPLC_PORTS_H_ */
