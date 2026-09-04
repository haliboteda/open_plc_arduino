/*
 * fw_verify.c -- see fw_verify.h.
 */

#include "fw_verify.h"
#include "uecc/uECC.h"

bool fw_verify_signature_with_key(const uint8_t pubkey[64],
		const uint8_t hash[32], const uint8_t signature[FW_SIGNATURE_SIZE])
{
	return uECC_verify(pubkey, hash, 32U, signature, uECC_secp256r1()) == 1;
}
