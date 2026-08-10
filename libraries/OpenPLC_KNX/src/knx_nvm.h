#pragma once

#include <stdint.h>
#include <stdbool.h>

/*
 * knx_nvm.h - Application non-volatile configuration for OpenPLC KNX.
 *
 * This structure holds application-level state that must survive power cycles:
 * relay output modes, last relay states, friendly name, etc.
 *
 * KNX stack internals (individual address, group address table, etc.) are
 * stored separately by the reference library via the Platform NVM interface
 * (see stm32h743_openplc_platform.cpp, KNX_STACK_NVM_* constants).
 *
 * Storage: STM32H743 internal Flash, Bank 2 Sector 7 (0x081E0000, 128 KB).
 * Only the first sizeof(KnxNvmConfig) bytes of that sector are used.
 */

#define KNX_NVM_MAGIC              0x4F504B58u  /* "OPKX" */
#define KNX_NVM_VERSION            1u
#define KNX_NVM_PROFILE_RELAY_2CH  1u
#define KNX_FRIENDLY_NAME_MAX_LEN  30u

typedef enum {
    KNX_POWER_UP_RESTORE = 0,   /* Restore last known relay state */
    KNX_POWER_UP_OFF     = 1,   /* Always open after power-up */
    KNX_POWER_UP_ON      = 2    /* Always closed after power-up */
} KnxPowerUpMode;

typedef struct {
    uint8_t power_up_mode;      /* KnxPowerUpMode */
    uint8_t feedback_enabled;   /* 1 = send status telegram on state change */
    uint8_t reserved[2];
} KnxRelayChannelConfig;

typedef struct {
    uint32_t              magic;
    uint16_t              version;
    uint16_t              payload_len;
    uint16_t              individual_addr;          /* KNX individual address (area.line.device) */
    uint8_t               profile_id;
    uint8_t               flags;
    uint16_t              manufacturer_id;
    uint16_t              device_version;
    uint16_t              project_installation_id;
    uint32_t              routing_multicast_addr;   /* KNXnet/IP multicast (host byte order) */
    uint8_t               ip_assignment_method;     /* 1 = DHCP */
    uint8_t               last_relay_state_bits;    /* bit N = state of relay channel N */
    uint8_t               serial_number[6];
    uint8_t               mac_address[6];
    char                  friendly_name[KNX_FRIENDLY_NAME_MAX_LEN];
    KnxRelayChannelConfig relay[2];
    uint16_t              checksum;
} KnxNvmConfig;

#ifdef __cplusplus
extern "C" {
#endif

void knx_nvm_set_defaults(KnxNvmConfig *config, uint16_t default_addr);
bool knx_nvm_load(KnxNvmConfig *config);
bool knx_nvm_save(KnxNvmConfig *config);
bool knx_nvm_is_valid(const KnxNvmConfig *config);

#ifdef __cplusplus
}
#endif
