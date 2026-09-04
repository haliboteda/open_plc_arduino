/*
 * iap_cert.h -- mirrors open_plc_cube_ide/IAPServer/iap_cert.h byte-for-byte.
 * See that file for the full design note.
 */

#ifndef OPENPLC_IAP_CERT_H_
#define OPENPLC_IAP_CERT_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IAP_CERT_SIZE       132U
#define IAP_CERT_SIGNED_LEN  68U

typedef struct {
	uint8_t  leaf_pubkey[64];
	uint32_t serial;
	uint8_t  root_sig[64];
} iap_cert_t;

_Static_assert(sizeof(iap_cert_t) == IAP_CERT_SIZE,
		"iap_cert_t must be exactly 132 bytes (64 + 4 + 64, no padding)");

bool iap_cert_verify(const iap_cert_t *cert, const uint8_t root[64]);

bool iap_cert_verify_image(const uint8_t hash[32], const uint8_t signature[64],
		const iap_cert_t *cert, const uint8_t root[64]);

#ifdef __cplusplus
}
#endif

#endif /* OPENPLC_IAP_CERT_H_ */
