/*
 * iap_auth.h
 *
 * ECDSA challenge-response for the "openplc_server_reboot" UDP command
 * (forces this device into bootloader/upload mode). Without this, anyone on
 * the network could remotely knock a running PLC off the air with a single
 * unauthenticated UDP packet.
 *
 * Mirrors open_plc_cube_ide/IAPServer/iap_auth.h (bootloader side) minus the
 * firmware-signature pieces, which don't apply here. The one real difference:
 * the bootloader checks a certificate against owner_slot_root() (in-process,
 * same flash it already owns); this side checks it against
 * owner_root_ro_get() (owner_root_ro.h), a read-only mirror of that same
 * resolution logic reading the bootloader's flash sector directly, because
 * there is no other channel that hands this app a trusted root.
 *
 * The app keeps no secret for this -- only public keys and certificates.
 * Why that shape was chosen: $PROD/docs/modules/M2-ownership.md.
 */

#ifndef OPENPLC_NET_IAP_AUTH_H_
#define OPENPLC_NET_IAP_AUTH_H_

#include <stdint.h>
#include <stdbool.h>
#include "iap_cert.h"

#ifdef __cplusplus
extern "C" {
#endif

#define IAP_AUTH_NONCE_SIZE   16U
#define IAP_AUTH_NONCE_TTL_MS 30000U

void iap_auth_issue_challenge(char *out_hex);

bool iap_auth_verify_and_consume(const uint8_t *msg, uint32_t msg_len,
		const iap_cert_t *cert, const uint8_t nonce_sig[64]);

#ifdef __cplusplus
}
#endif

#endif /* OPENPLC_NET_IAP_AUTH_H_ */
