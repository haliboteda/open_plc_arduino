#include "knx_nvm.h"
#include "knx_config.h"
#include <string.h>

/*
 * Application NVM and the KNX stack NVM share one flash sector (layout in
 * knx_config.h). The STM32H743 programs 256-bit (32-byte) flash words; reads
 * are memory-mapped, so a load is a memcpy.
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
 * Save - both blocks share one erase unit, so every save rewrites both
 * ---------------------------------------------------------------------- */

/* Programs 32-byte flash words; words that are all 0xFF are left erased. */
static bool knx_flash_write(uint32_t addr, const uint8_t *data, uint32_t size)
{
    /* Aligned buffer; must stay valid until HAL_FLASH_Program returns */
    uint8_t buf[32u] __attribute__((aligned(32u)));
    uint8_t erased[32u];
    memset(erased, 0xFFu, sizeof(erased));

    for (uint32_t offset = 0u; offset < size; offset += 32u) {
        uint32_t chunk = (size - offset > 32u) ? 32u : (size - offset);
        memset(buf, 0xFFu, sizeof(buf));
        memcpy(buf, data + offset, chunk);
        if (memcmp(buf, erased, sizeof(buf)) == 0) continue;
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD, addr + offset,
                              (uint32_t)(uintptr_t)buf) != HAL_OK) {
            return false;
        }
    }
    return true;
}

bool knx_nvm_sector_write(const uint8_t *stack, uint32_t stack_len,
                          const KnxNvmConfig *app)
{
    /* Copies of the block the caller is not changing, taken before the erase. */
    static uint8_t      s_stack[KNX_FLASH_SIZE];
    static KnxNvmConfig s_app;

    if (stack == NULL) {
        memcpy(s_stack, (const void *)KNX_STACK_NVM_FLASH_ADDR, sizeof(s_stack));
        stack     = s_stack;
        stack_len = sizeof(s_stack);
    }
    if (stack_len > KNX_FLASH_SIZE) return false;
    if (app == NULL) {
        memcpy(&s_app, (const void *)KNX_APP_NVM_FLASH_ADDR, sizeof(s_app));
        app = &s_app;
    }

    HAL_FLASH_Unlock();

    FLASH_EraseInitTypeDef eraseInit;
    eraseInit.TypeErase = FLASH_TYPEERASE_SECTORS;
    eraseInit.Banks     = KNX_NVM_FLASH_BANK;
    eraseInit.Sector    = KNX_NVM_FLASH_SECTOR;
    eraseInit.NbSectors = 1u;

    uint32_t sectorError = 0u;
    bool ok = (HAL_FLASHEx_Erase(&eraseInit, &sectorError) == HAL_OK)
           && knx_flash_write(KNX_STACK_NVM_FLASH_ADDR, stack, stack_len)
           && knx_flash_write(KNX_APP_NVM_FLASH_ADDR, (const uint8_t *)app,
                              sizeof(KnxNvmConfig));

    HAL_FLASH_Lock();
    return ok;
}

bool knx_nvm_save(KnxNvmConfig *config)
{
    if (config == NULL) return false;

    config->checksum = knx_nvm_checksum(config);
    return knx_nvm_sector_write(NULL, 0u, config);
}
