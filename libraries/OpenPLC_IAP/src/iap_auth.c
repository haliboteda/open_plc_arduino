/*
 * iap_auth.c
 *
 * See iap_auth.h. Mirrors open_plc_cube_ide/IAPServer/iap_auth.c. The nonce is
 * 16 bytes straight from the RNG peripheral -- no backup register, nothing that
 * has to survive a power cycle. See $PROD/docs/tables/DECISIONS.md, decision 66.
 */

#include "iap_auth.h"
#include "iap_cert.h"
#include "owner_root_ro.h"
#include "fw_verify.h"
#include "sha256.h"
#include "Arduino.h"
#include "stm32_def.h"
#include <string.h>
#include <stdio.h>

static uint8_t  s_nonce[IAP_AUTH_NONCE_SIZE];
static bool     s_nonce_pending;
static uint32_t s_nonce_issue_tick;

/* The Arduino core brings up no RNG of its own, so this file owns the handle and
 * the clock, and initialises both on first use. The kernel clock is HSI48, which
 * SystemClock_Config() already turns on for USB (variants/STM32H7xx/H743/
 * generic_clock.c) -- a sketch that replaces that WEAK function has to keep it.
 *
 * Deliberately different from the bootloader copy, which uses the handle CubeMX
 * generates. Only iap_auth_issue_challenge() is compared across the repositories. */
static RNG_HandleTypeDef s_hrng;
static bool s_rng_ready;

static bool rng_words(uint32_t *words, uint32_t n)
{
	uint32_t i;

	if (!s_rng_ready) {
		RCC_PeriphCLKInitTypeDef clk = {0};

		clk.PeriphClockSelection = RCC_PERIPHCLK_RNG;
		clk.RngClockSelection = RCC_RNGCLKSOURCE_HSI48;
		if (HAL_RCCEx_PeriphCLKConfig(&clk) != HAL_OK) {
			return false;
		}
		__HAL_RCC_RNG_CLK_ENABLE();

		s_hrng.Instance = RNG;
		s_hrng.Init.ClockErrorDetection = RNG_CED_ENABLE;
		if (HAL_RNG_Init(&s_hrng) != HAL_OK) {
			return false;
		}
		s_rng_ready = true;
	}

	for (i = 0; i < n; i++) {
		if (HAL_RNG_GenerateRandomNumber(&s_hrng, &words[i]) != HAL_OK) {
			return false;
		}
	}
	return true;
}

bool iap_auth_issue_challenge(char *out_hex)
{
	uint32_t words[IAP_AUTH_NONCE_SIZE / 4U];
	uint32_t i;

	/* Dropped before the RNG is asked: a failed attempt must not leave the
	 * previous nonce accepting answers. */
	s_nonce_pending = false;

	if (!rng_words(words, IAP_AUTH_NONCE_SIZE / 4U)) {
		return false;
	}
	memcpy(s_nonce, words, IAP_AUTH_NONCE_SIZE);

	s_nonce_pending = true;
	s_nonce_issue_tick = HAL_GetTick();

	for (i = 0; i < IAP_AUTH_NONCE_SIZE; i++) {
		sprintf(out_hex + i * 2U, "%02x", s_nonce[i]);
	}
	out_hex[IAP_AUTH_NONCE_SIZE * 2U] = '\0';
	return true;
}

bool iap_auth_verify_and_consume(const uint8_t *msg, uint32_t msg_len,
		const iap_cert_t *cert, const uint8_t nonce_sig[64])
{
	uint8_t buf[IAP_AUTH_NONCE_SIZE + 256U];
	uint8_t digest[SHA256_DIGEST_SIZE];
	uint8_t root[64];

	if (!s_nonce_pending) {
		return false;
	}
	s_nonce_pending = false; /* one-shot: consumed whether this check passes or not */

	if ((HAL_GetTick() - s_nonce_issue_tick) > IAP_AUTH_NONCE_TTL_MS) {
		printf("Auth nonce expired\r\n");
		return false;
	}
	if (msg_len > sizeof(buf) - IAP_AUTH_NONCE_SIZE) {
		return false;
	}

	owner_root_ro_get(root);
	if (!iap_cert_verify(cert, root, owner_root_ro_is_revoked(cert->leaf_pubkey))) {
		return false;
	}

	memcpy(buf, s_nonce, IAP_AUTH_NONCE_SIZE);
	memcpy(buf + IAP_AUTH_NONCE_SIZE, msg, msg_len);
	sha256(buf, IAP_AUTH_NONCE_SIZE + msg_len, digest);

	return fw_verify_signature_with_key(cert->leaf_pubkey, digest, nonce_sig);
}
