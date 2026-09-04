/*
 * iap_keyderive.h
 *
 * This device's machine ID (STM32 96-bit UID) -- used for UDP discovery/ping
 * identity strings and (see owner_root_ro.c) checking an owner record's uid
 * field.
 *
 * Must match, byte-for-byte:
 *   - open_plc_cube_ide/IAPServer/iap_keyderive.h/.c (bootloader side)
 */

#ifndef OPENPLC_NET_IAP_KEYDERIVE_H_
#define OPENPLC_NET_IAP_KEYDERIVE_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IAP_MACHINE_ID_SIZE    12U /* STM32 96-bit UID: UIDW2||UIDW1||UIDW0 */
#define IAP_MACHINE_ID_HEX_LEN (IAP_MACHINE_ID_SIZE * 2U)

/* This device's machine ID as raw bytes, big-endian UIDW2||UIDW1||UIDW0. */
void iap_keyderive_get_machine_id(uint8_t out_id[IAP_MACHINE_ID_SIZE]);

/* This device's machine ID as an uppercase hex string (as sent in UDP
 * discovery/ping replies), null-terminated. */
void iap_keyderive_get_machine_id_hex(char out_hex[IAP_MACHINE_ID_HEX_LEN + 1U]);

#ifdef __cplusplus
}
#endif

#endif /* OPENPLC_NET_IAP_KEYDERIVE_H_ */
