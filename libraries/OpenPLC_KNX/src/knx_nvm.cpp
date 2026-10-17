#include "knx_nvm.h"
#include "knx_config.h"
#include <string.h>

/* The STM32H743 programs 256-bit (32-byte) flash words. */

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

bool knx_nvm_sector_write(const uint8_t *stack, uint32_t stack_len)
{
    if (stack == NULL || stack_len > KNX_FLASH_SIZE) return false;

    HAL_FLASH_Unlock();

    // VoltageRange is ORed into FLASH_CR; a stale value can set BER and erase
    // the whole bank, bootloader state included. See KEX-13 in $PROD.
    FLASH_EraseInitTypeDef eraseInit = {0};
    eraseInit.TypeErase    = FLASH_TYPEERASE_SECTORS;
    eraseInit.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    eraseInit.Banks        = KNX_NVM_FLASH_BANK;
    eraseInit.Sector       = KNX_NVM_FLASH_SECTOR;
    eraseInit.NbSectors    = 1u;

    // Header word last: a write cut by power loss leaves the header erased,
    // which the stack reads as invalid data. See $PROD M3, R3-20.
    uint32_t head = (stack_len > 32u) ? 32u : stack_len;
    uint32_t sectorError = 0u;
    bool ok = (HAL_FLASHEx_Erase(&eraseInit, &sectorError) == HAL_OK)
           && knx_flash_write(KNX_STACK_NVM_FLASH_ADDR + head, stack + head, stack_len - head)
           && knx_flash_write(KNX_STACK_NVM_FLASH_ADDR, stack, head);

    HAL_FLASH_Lock();
    return ok;
}
