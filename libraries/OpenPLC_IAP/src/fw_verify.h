/*
 * fw_verify.h -- ECDSA (secp256r1) signature verification, using the vendored
 * micro-ecc library (uecc/). Mirrors the one function of
 * open_plc_cube_ide/IAPServer/fw_verify.h this side needs.
 *
 * No fw_verify_signature() here (the bootloader's convenience wrapper around
 * "the root this board trusts") -- this side's equivalent question is
 * answered by owner_root_ro_get(), not by a same-named function, so callers
 * always go through fw_verify_signature_with_key() explicitly.
 */

#ifndef OPENPLC_IAP_FW_VERIFY_H_
#define OPENPLC_IAP_FW_VERIFY_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FW_PUBLIC_KEY_SIZE 64U
#define FW_SIGNATURE_SIZE  64U

bool fw_verify_signature_with_key(const uint8_t pubkey[64],
		const uint8_t hash[32], const uint8_t signature[FW_SIGNATURE_SIZE]);

#ifdef __cplusplus
}
#endif

#endif /* OPENPLC_IAP_FW_VERIFY_H_ */
