#pragma once

#include <stdint.h>
#include <stdbool.h>

/*
 * knx_nvm.h - writes the KNX stack NVM (the ETS data) to its flash sector;
 * layout in knx_config.h. Called by the Platform NVM interface
 * (stm32h743_openplc_platform.cpp). The board package's postbuild step looks
 * for this symbol to apply the KNX image size limit, so keep its name.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* Erases the KNX sector and writes stack[0..stack_len) at its start.
 * stack_len must not exceed KNX_FLASH_SIZE. */
bool knx_nvm_sector_write(const uint8_t *stack, uint32_t stack_len);

#ifdef __cplusplus
}
#endif
