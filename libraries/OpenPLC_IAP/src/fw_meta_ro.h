/*
 * fw_meta_ro.h -- read-only view of the firmware metadata the bootloader keeps
 * in sector 15, for the app's answer to "getapprevoked".
 *
 * Read-only for the same reason as owner_root_ro.h: only the bootloader writes
 * sector 15. Must match the record layout and scan in
 * open_plc_cube_ide/IAPServer/bootloader_state.c (mirror 21 in
 * $PROD/docs/repo/ARCHITECTURE.md).
 */

#ifndef OPENPLC_IAP_FW_META_RO_H_
#define OPENPLC_IAP_FW_META_RO_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Leaf public key of the certificate in the latest metadata record. False
 * when there is no record or it names an empty image. */
bool fw_meta_ro_installed_leaf(uint8_t out[64]);

#ifdef __cplusplus
}
#endif

#endif /* OPENPLC_IAP_FW_META_RO_H_ */
