/*
 * iap_keyderive.h
 *
 * Mirrors open_plc_cube_ide/IAPServer/iap_keyderive.h. See that file for the
 * full design note: derives each device's own IAP auth key from one fixed
 * shared password mixed with its machine ID (STM32 96-bit UID), isolated
 * here so a future stronger scheme only requires changing
 * iap_keyderive_get_device_key()'s body.
 *
 * Must match, byte-for-byte:
 *   - open_plc_cube_ide/IAPServer/iap_keyderive.h/.c (bootloader side)
 *   - IAPTranfer_Tool/iapcrypto (PC tool, Go)
 */

#ifndef OPENPLC_NET_IAP_KEYDERIVE_H_
#define OPENPLC_NET_IAP_KEYDERIVE_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IAP_DEVICE_KEY_SIZE    32U
#define IAP_MACHINE_ID_SIZE    12U /* STM32 96-bit UID: UIDW2||UIDW1||UIDW0 */
#define IAP_MACHINE_ID_HEX_LEN (IAP_MACHINE_ID_SIZE * 2U)

/* This device's machine ID as raw bytes, big-endian UIDW2||UIDW1||UIDW0. */
void iap_keyderive_get_machine_id(uint8_t out_id[IAP_MACHINE_ID_SIZE]);

/* This device's machine ID as an uppercase hex string (as sent in UDP
 * discovery/ping replies), null-terminated. */
void iap_keyderive_get_machine_id_hex(char out_hex[IAP_MACHINE_ID_HEX_LEN + 1U]);

/* Derives this device's own key: HMAC-SHA256(fixed_password, machine_id). */
void iap_keyderive_get_device_key(uint8_t out_key[IAP_DEVICE_KEY_SIZE]);

#ifdef __cplusplus
}
#endif

#endif /* OPENPLC_NET_IAP_KEYDERIVE_H_ */
