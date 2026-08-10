/*
 * iap_auth.h
 *
 * HMAC-SHA256 challenge-response for the "openplc_server_reboot" UDP
 * command (forces this device into bootloader/upload mode). Without this,
 * anyone on the network could remotely knock a running PLC off the air with
 * a single unauthenticated UDP packet.
 *
 * Mirrors open_plc_cube_ide/IAPServer/iap_auth.h (bootloader side) minus the
 * firmware-signature pieces, which don't apply here. The key itself is
 * per-device: see iap_keyderive.h. It is derived from a fixed shared
 * password mixed with this device's machine ID (UID), so every device
 * authenticates with a different effective key even though the same
 * password is embedded in every firmware image. See
 * IAPServer/keys/README.md for provisioning notes.
 */

#ifndef OPENPLC_NET_IAP_AUTH_H_
#define OPENPLC_NET_IAP_AUTH_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IAP_AUTH_NONCE_SIZE   16U
#define IAP_AUTH_HMAC_SIZE    32U
#define IAP_AUTH_NONCE_TTL_MS 30000U

void iap_auth_issue_challenge(char *out_hex);
bool iap_auth_verify_and_consume(const uint8_t *msg, uint32_t msg_len, const uint8_t hmac[IAP_AUTH_HMAC_SIZE]);

#ifdef __cplusplus
}
#endif

#endif /* OPENPLC_NET_IAP_AUTH_H_ */
