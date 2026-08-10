#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stm32h7xx_hal.h>
#include "knx_nvm.h"

/*
 * knx_profiles.h - OpenPLC device profiles (relay channels, etc.)
 *
 * All GPIO is accessed via HAL_GPIO_WritePin / HAL_GPIO_ReadPin.
 * No Arduino.h dependency.
 *
 * User-facing code (Arduino sketches) can continue to use Arduino GPIO macros
 * for their own pins - this header only controls the profile outputs.
 */

/* HAL GPIO port + pin pair */
typedef struct {
    GPIO_TypeDef *port;
    uint16_t      pin;
} KnxHalPin;

typedef struct {
    const char  *name;
    uint8_t      channel_count;
    KnxHalPin    relay_pins[2];     /* HAL port/pin for each relay coil */
    uint16_t     switch_ga[2];      /* Default group addr for switch commands */
    uint16_t     status_ga[2];      /* Default group addr for status feedback */
} KnxRelayProfile;

#ifdef __cplusplus
extern "C" {
#endif

/* Returns the built-in 2-channel relay profile (relay outputs on PE6/PE5). */
const KnxRelayProfile *knx_profile_relay_2ch(void);

/* Initialise relay GPIO and set outputs to their power-up states from config. */
void knx_profile_apply_defaults(const KnxRelayProfile *profile,
                                const KnxNvmConfig    *config);

/* Drive relay output on/off.
 * Returns false if profile is NULL or channel index is out of range. */
bool knx_profile_set_channel(const KnxRelayProfile *profile,
                              uint8_t channel,
                              bool on);

/* Read relay output state (reflects last written state, not coil current). */
bool knx_profile_channel_state(const KnxRelayProfile *profile,
                                uint8_t channel);

#ifdef __cplusplus
}
#endif
