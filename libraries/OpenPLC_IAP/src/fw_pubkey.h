/*
 * fw_pubkey.h -- mirrors open_plc_cube_ide/IAPServer/fw_pubkey.h.
 *
 * The factory-published root public key, compiled in as a fallback for when
 * owner_root_ro.c finds no valid owner record. Rotate with
 * open_plc_cube_ide/IAPServer/keys/rotate_keys.sh (keeps this file in sync
 * with the bootloader's copy).
 */

#ifndef OPENPLC_IAP_FW_PUBKEY_H_
#define OPENPLC_IAP_FW_PUBKEY_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const uint8_t fw_public_key[64];

#ifdef __cplusplus
}
#endif

#endif /* OPENPLC_IAP_FW_PUBKEY_H_ */
