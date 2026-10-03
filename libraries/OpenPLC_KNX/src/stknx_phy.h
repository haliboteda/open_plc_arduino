#pragma once

/*
 * stknx_phy.h - the two timers that make and capture TP1 bits for the STKNX.
 *
 * TIM12_CH1 drives KNX_TX, TIM1_CH3 timestamps KNX_RX falling edges; both
 * interrupts feed stknx_tp1. Pins and polarity:
 * $PROD/docs/hardware/HARDWARE-FACTS.md "KNX 接口".
 */

#include "stknx_tp1.h"

/* KNX_TX as a plain output driven low - the transceiver's idle. A floating
 * KNX_TX makes the STKNX draw current from the bus. */
void stknx_phy_park(void);

/* Takes TIM1 and TIM12 and starts the bit engine for l. false = already running. */
bool stknx_phy_start(stknx_link_t *l);

/* Stops both timers and parks KNX_TX. */
void stknx_phy_stop(void);
