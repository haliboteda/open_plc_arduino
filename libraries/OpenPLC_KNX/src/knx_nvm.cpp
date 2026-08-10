#include "knx_nvm.h"
#include "knx_config.h"
#include <string.h>

/*
 * Application NVM stored in STM32H743 internal Flash:
 *   Bank 2, Sector 7 - 0x081E0000, 128 KB sector
 *
 * The STM32H743 requires 256-bit (32-byte) aligned writes.
 * On read, the Flash is directly memory-mapped so we just memcpy.
 * On write, we erase the whole sector then reprogram in 32-byte chunks.
 *
 * KNX stack NVM (ETS programming data) lives in Bank 2 Sector 6 (0x081C0000)
 * and is managed independently by stm32h743_openplc_platform.cpp.
 */

/* -------------------------------------------------------------------------
 * Checksum
 * ---------------------------------------------------------------------- */

static uint16_t knx_nvm_checksum(const KnxNvmConfig *config)
{
    const uint8_t *bytes = (const uint8_t *)config;
    uint16_t sum = 0u;
    uint16_t len = (uint16_t)(sizeof(KnxNvmConfig) - sizeof(config->checksum));
    for (uint16_t i = 0u; i < len; i++) {
        sum = (uint16_t)(sum + bytes[i]);
    }
    return sum;
}

/* -------------------------------------------------------------------------
 * Defaults
 * ---------------------------------------------------------------------- */

void knx_nvm_set_defaults(KnxNvmConfig *config, uint16_t default_addr)
{
    if (config == NULL) return;

    memset(config, 0, sizeof(*config));
    config->magic                    = KNX_NVM_MAGIC;
    config->version                  = KNX_NVM_VERSION;
    config->payload_len              = (uint16_t)(sizeof(KnxNvmConfig) - 8u);
    config->individual_addr          = default_addr;
    config->profile_id               = KNX_NVM_PROFILE_RELAY_2CH;
    config->manufacturer_id          = 0x00FAu;
    config->device_version           = 0x0100u;
    config->routing_multicast_addr   = 0xE000170Cu; /* 224.0.23.12, host byte order */
    config->ip_assignment_method     = 1u;           /* DHCP */
    config->serial_number[1]         = 0xFAu;
    config->serial_number[2]         = 0x01u;
    config->serial_number[3]         = 0x02u;
    config->serial_number[4]         = 0x03u;
    config->serial_number[5]         = 0x04u;
    strncpy(config->friendly_name, "OpenPLC KNX",
            sizeof(config->friendly_name) - 1u);
    config->relay[0].power_up_mode    = KNX_POWER_UP_RESTORE;
    config->relay[1].power_up_mode    = KNX_POWER_UP_RESTORE;
    config->relay[0].feedback_enabled = 1u;
    config->relay[1].feedback_enabled = 1u;
    config->checksum = knx_nvm_checksum(config);
}

/* -------------------------------------------------------------------------
 * Validation
 * ---------------------------------------------------------------------- */

bool knx_nvm_is_valid(const KnxNvmConfig *config)
{
    if (config == NULL)                              return false;
    if (config->magic   != KNX_NVM_MAGIC)           return false;
    if (config->version != KNX_NVM_VERSION)         return false;
    return (config->checksum == knx_nvm_checksum(config));
}

/* -------------------------------------------------------------------------
 * Load - direct memory-mapped Flash read
 * ---------------------------------------------------------------------- */

bool knx_nvm_load(KnxNvmConfig *config)
{
    if (config == NULL) return false;
    memcpy(config, (const void *)KNX_APP_NVM_FLASH_ADDR, sizeof(KnxNvmConfig));
    return knx_nvm_is_valid(config);
}

/* -------------------------------------------------------------------------
 * Save - erase sector, write 32-byte aligned chunks
 * ---------------------------------------------------------------------- */

bool knx_nvm_save(KnxNvmConfig *config)
{
    if (config == NULL) return false;

    config->checksum = knx_nvm_checksum(config);

    HAL_FLASH_Unlock();

    /* Erase Bank 2 Sector 7 */
    FLASH_EraseInitTypeDef eraseInit;
    eraseInit.TypeErase = FLASH_TYPEERASE_SECTORS;
    eraseInit.Banks     = KNX_APP_NVM_FLASH_BANK;
    eraseInit.Sector    = KNX_APP_NVM_FLASH_SECTOR;
    eraseInit.NbSectors = 1u;

    uint32_t sectorError = 0u;
    if (HAL_FLASHEx_Erase(&eraseInit, &sectorError) != HAL_OK) {
        HAL_FLASH_Lock();
        return false;
    }

    /* Write in 32-byte (256-bit) aligned chunks */
    const uint8_t *src    = (const uint8_t *)config;
    uint32_t       addr   = KNX_APP_NVM_FLASH_ADDR;
    size_t         total  = sizeof(KnxNvmConfig);
    size_t         offset = 0u;

    /* Aligned buffer on stack; must stay valid until HAL_FLASH_Program returns */
    uint8_t buf[32u] __attribute__((aligned(32u)));

    while (offset < total) {
        memset(buf, 0xFFu, sizeof(buf));
        size_t chunk = (total - offset > 32u) ? 32u : (total - offset);
        memcpy(buf, src + offset, chunk);

        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD,
                              addr + offset,
                              (uint32_t)(uintptr_t)buf) != HAL_OK) {
            HAL_FLASH_Lock();
            return false;
        }
        offset += 32u;
    }

    HAL_FLASH_Lock();
    return true;
}
