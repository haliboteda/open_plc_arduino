#include "knx_profiles.h"
#include "knx_config.h"

/*
 * Built-in 2-channel relay profile.
 *
 * Physical outputs:
 *   Channel 0 → PE6 (REL_1 on Bridge MPU schematic)
 *   Channel 1 → PE5 (REL_2 on Bridge MPU schematic)
 *
 * Default group addresses are factory defaults; ETS overwrites these
 * via the standard KNX commissioning flow.
 *
 * KNX_RELAY_ACTIVE / KNX_RELAY_INACTIVE are defined in knx_config.h
 * (GPIO_PIN_SET / GPIO_PIN_RESET respectively).
 */

static const KnxRelayProfile s_relay_2ch_profile = {
    /* name          */ "Relay2CH",
    /* channel_count */ 2u,
    /* relay_pins    */ {
        { KNX_RELAY1_PORT, KNX_RELAY1_PIN },
        { KNX_RELAY2_PORT, KNX_RELAY2_PIN },
    },
    /* switch_ga     */ { 0x0001u, 0x0002u },   /* 0/0/1 and 0/0/2 */
    /* status_ga     */ { 0x0101u, 0x0102u },   /* 0/1/1 and 0/1/2 */
};

/* -------------------------------------------------------------------------
 * Internal helpers
 * ---------------------------------------------------------------------- */

static bool power_up_output(const KnxRelayChannelConfig *ch, uint8_t ch_idx,
                             const KnxNvmConfig *config)
{
    switch ((KnxPowerUpMode)ch->power_up_mode) {
        case KNX_POWER_UP_ON:
            return true;
        case KNX_POWER_UP_OFF:
            return false;
        case KNX_POWER_UP_RESTORE:
        default:
            return ((config->last_relay_state_bits >> ch_idx) & 0x01u) != 0u;
    }
}

/* -------------------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------------- */

const KnxRelayProfile *knx_profile_relay_2ch(void)
{
    return &s_relay_2ch_profile;
}

void knx_profile_apply_defaults(const KnxRelayProfile *profile,
                                const KnxNvmConfig    *config)
{
    if (profile == NULL || config == NULL) return;

    /* Enable GPIO clocks for relay outputs (REL_1=PI8, REL_2=PI10) */
    __HAL_RCC_GPIOI_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();   /* REL_4=PG7, REL_5=PG3 */
    __HAL_RCC_GPIOD_CLK_ENABLE();   /* REL_6=PD3 */

    for (uint8_t i = 0u; i < profile->channel_count; i++) {
        const KnxHalPin *pin = &profile->relay_pins[i];

        /* Configure as push-pull output, no pull, slow speed */
        GPIO_InitTypeDef init = {0};
        init.Pin   = pin->pin;
        init.Mode  = GPIO_MODE_OUTPUT_PP;
        init.Pull  = GPIO_NOPULL;
        init.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(pin->port, &init);

        bool on = power_up_output(&config->relay[i], i, config);
        HAL_GPIO_WritePin(pin->port, pin->pin,
                          on ? KNX_RELAY_ACTIVE : KNX_RELAY_INACTIVE);
    }
}

bool knx_profile_set_channel(const KnxRelayProfile *profile,
                              uint8_t channel,
                              bool on)
{
    if (profile == NULL || channel >= profile->channel_count) return false;
    const KnxHalPin *pin = &profile->relay_pins[channel];
    HAL_GPIO_WritePin(pin->port, pin->pin,
                      on ? KNX_RELAY_ACTIVE : KNX_RELAY_INACTIVE);
    return true;
}

bool knx_profile_channel_state(const KnxRelayProfile *profile,
                                uint8_t channel)
{
    if (profile == NULL || channel >= profile->channel_count) return false;
    const KnxHalPin *pin = &profile->relay_pins[channel];
    return HAL_GPIO_ReadPin(pin->port, pin->pin) == KNX_RELAY_ACTIVE;
}
