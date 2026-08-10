/*
 * iap_auth.c
 *
 * See iap_auth.h. Mirrors open_plc_cube_ide/IAPServer/iap_auth.c; uses a
 * different RTC backup register (DR2 vs the bootloader's DR1) purely to
 * keep the two counters visually separate in a debugger -- the bootloader
 * and this app image never run at the same time, so there's no real
 * collision risk either way.
 */

#include "iap_auth.h"
#include "iap_keyderive.h"
#include "sha256.h"
#include "Arduino.h"
#include "stm32_def.h"
#include "rtc.h"
#include <string.h>
#include <stdio.h>

#ifndef IAP_AUTH_COUNTER_BKP_REG
#define IAP_AUTH_COUNTER_BKP_REG RTC_BKP_DR2
#endif

static uint8_t  s_nonce[IAP_AUTH_NONCE_SIZE];
static bool     s_nonce_pending;
static uint32_t s_nonce_issue_tick;

static uint32_t next_counter(void)
{
	uint32_t v = HAL_RTCEx_BKUPRead(&hrtc, IAP_AUTH_COUNTER_BKP_REG) + 1U;
	HAL_PWR_EnableBkUpAccess();
	HAL_RTCEx_BKUPWrite(&hrtc, IAP_AUTH_COUNTER_BKP_REG, v);
	HAL_PWR_DisableBkUpAccess();
	return v;
}

static bool constant_time_eq(const uint8_t *a, const uint8_t *b, uint32_t len)
{
	uint8_t diff = 0;
	uint32_t i;
	for (i = 0; i < len; i++) {
		diff |= a[i] ^ b[i];
	}
	return diff == 0U;
}

void iap_auth_issue_challenge(char *out_hex)
{
	uint32_t counter = next_counter();
	uint32_t uid0 = HAL_GetUIDw0();
	uint32_t tick = HAL_GetTick();
	uint32_t i;

	memcpy(s_nonce, &counter, 4U);
	memcpy(s_nonce + 4, &uid0, 4U);
	memcpy(s_nonce + 8, &tick, 4U);
	memset(s_nonce + 12, 0, 4U);

	s_nonce_pending = true;
	s_nonce_issue_tick = tick;

	for (i = 0; i < IAP_AUTH_NONCE_SIZE; i++) {
		sprintf(out_hex + i * 2U, "%02x", s_nonce[i]);
	}
	out_hex[IAP_AUTH_NONCE_SIZE * 2U] = '\0';
}

bool iap_auth_verify_and_consume(const uint8_t *msg, uint32_t msg_len, const uint8_t hmac[IAP_AUTH_HMAC_SIZE])
{
	uint8_t buf[IAP_AUTH_NONCE_SIZE + 256U];
	uint8_t calc[IAP_AUTH_HMAC_SIZE];
	uint8_t device_key[IAP_DEVICE_KEY_SIZE];

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

	memcpy(buf, s_nonce, IAP_AUTH_NONCE_SIZE);
	memcpy(buf + IAP_AUTH_NONCE_SIZE, msg, msg_len);

	iap_keyderive_get_device_key(device_key);
	hmac_sha256(device_key, sizeof(device_key), buf, IAP_AUTH_NONCE_SIZE + msg_len, calc);

	return constant_time_eq(calc, hmac, IAP_AUTH_HMAC_SIZE);
}
