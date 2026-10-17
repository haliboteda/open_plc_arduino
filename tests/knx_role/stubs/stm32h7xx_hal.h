/* Host stand-in for the STM32H7 HAL: only what knx_config.h and knx_nvm.cpp
 * use. The fake flash behind it lives in test_main.cpp. */
#pragma once

#include <stdint.h>

typedef enum { HAL_OK = 0x00, HAL_ERROR = 0x01 } HAL_StatusTypeDef;

typedef struct {
    uint32_t TypeErase;
    uint32_t Banks;
    uint32_t Sector;
    uint32_t NbSectors;
    uint32_t VoltageRange;
} FLASH_EraseInitTypeDef;

#define FLASH_TYPEERASE_SECTORS      0x00u
#define FLASH_VOLTAGE_RANGE_3        0x20u
#define FLASH_BANK_2                 0x02u
#define FLASH_SECTOR_6               6u
#define FLASH_TYPEPROGRAM_FLASHWORD  0x01u

HAL_StatusTypeDef HAL_FLASH_Unlock(void);
HAL_StatusTypeDef HAL_FLASH_Lock(void);
HAL_StatusTypeDef HAL_FLASHEx_Erase(FLASH_EraseInitTypeDef *init, uint32_t *sectorError);
HAL_StatusTypeDef HAL_FLASH_Program(uint32_t typeProgram, uint32_t flashAddress, uint32_t dataAddress);
