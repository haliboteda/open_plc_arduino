/*
 * T3-07: AI / AO in mV and mA apply this board's calibration, and fall back to
 * the nominal conversion -- logging once -- when the calibration area is
 * blank, corrupt or another board's.
 *
 * Runs the REAL openplc_calib.c and openplc_analog.c over a RAM copy of the
 * calibration area. The area is built here by byte offset (format:
 * $PROD/docs/modules/M1/SECTOR-15.md, "校准值区的格式") with an independent
 * CRC, so a layout change in the library shows up as a failure.
 *
 * One process per case: openplc_calib_get() reads the area once per boot.
 *   calib_test ok | blank | corrupt | other
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "openplc_analog.h"

_Alignas(8) uint8_t test_calib_area[64];
const uint32_t test_uid[3] = {0x11111111U, 0x22222222U, 0x33333333U};

static int log_lines;
void test_log(const char *msg)
{
	log_lines++;
	printf("  log: %s\n", msg);
}

static int failures;
static void check(int ok, const char *what)
{
	printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
	if (!ok) {
		failures++;
	}
}

static int near(float a, float b)
{
	return fabsf(a - b) <= 1e-3f * (fabsf(b) + 1.0f);
}

static void put32(uint32_t off, uint32_t v)
{
	memcpy(&test_calib_area[off], &v, 4);
}

static void putf(uint32_t off, float v)
{
	memcpy(&test_calib_area[off], &v, 4);
}

/* Reflected CRC-32 (IEEE), written independently of the library's. */
static uint32_t crc32(const uint8_t *p, uint32_t n)
{
	uint32_t c = 0xFFFFFFFFU;
	for (uint32_t i = 0; i < n; i++) {
		c ^= p[i];
		for (int k = 0; k < 8; k++) {
			c = (c & 1U) ? (c >> 1) ^ 0xEDB88320U : c >> 1;
		}
	}
	return ~c;
}

/* gain/offset per channel AI1, AI2, AO1, AO2 */
static const float GAIN[4] = {1.02f, 0.97f, 1.05f, 0.95f};
static const float OFFS[4] = {15.0f, 0.04f, -0.10f, 0.20f};

static void build_valid_area(void)
{
	memset(test_calib_area, 0xFF, sizeof(test_calib_area));
	put32(0, 0x4C41434FU);                          /* magic "OCAL" */
	test_calib_area[4] = 1; test_calib_area[5] = 0; /* version */
	test_calib_area[6] = 4; test_calib_area[7] = 0; /* channels */
	put32(8, test_uid[0]); put32(12, test_uid[1]); put32(16, test_uid[2]);
	for (int i = 0; i < 4; i++) {
		putf(20U + 8U * (uint32_t)i, GAIN[i]);
		putf(24U + 8U * (uint32_t)i, OFFS[i]);
	}
	put32(52, crc32(test_calib_area, 52));
}

/* Nominal values the library's conversion must start from. */
static float nominal_ai1(uint32_t raw) { return raw * 2500.0f / 4095.0f * (90.6f / 22.6f); }
static float nominal_ai2(uint32_t raw) { return raw * 2500.0f / 4095.0f / 124.0f; }

static void expect_calibrated(void)
{
	calib_status_t st;
	const calib_area_t *a = openplc_calib_get(&st);
	check(st == CALIB_OK, "status is CALIB_OK");

	float ai1 = openplc_ai1_mv_from_raw(2048, &a->ch[CALIB_CH_AI1]);
	check(near(ai1, GAIN[0] * nominal_ai1(2048) + OFFS[0]), "AI1 mV = gain * nominal + offset");
	float ai2 = openplc_ai2_ma_from_raw(3000, &a->ch[CALIB_CH_AI2]);
	check(near(ai2, GAIN[1] * nominal_ai2(3000) + OFFS[1]), "AI2 mA = gain * nominal + offset");

	/* Output: the hardware gets (target - offset) / gain; 10 mA nominal is
	 * 1024 mV at the XTR111 input. */
	uint32_t code = openplc_ao_code_from_ma(10.0f, &a->ch[CALIB_CH_AO1]);
	float want = (10.0f - OFFS[2]) / GAIN[2] * 102.4f * 4095.0f / 2500.0f;
	check((float)code > want - 1.0f && (float)code < want + 1.0f, "AO1 code = (target - offset) / gain, converted");
	check(openplc_ao_code_from_ma(25.0f, &a->ch[CALIB_CH_AO2]) ==
	      openplc_ao_code_from_ma(20.0f, &a->ch[CALIB_CH_AO2]), "AO above 20 mA is clamped to 20 mA");
	check(openplc_ao_code_from_ma(-3.0f, &a->ch[CALIB_CH_AO2]) ==
	      openplc_ao_code_from_ma(0.0f, &a->ch[CALIB_CH_AO2]), "AO below 0 mA is clamped to 0 mA");
	check(log_lines == 0, "a valid area logs nothing");
}

static void expect_nominal(calib_status_t want, const char *name)
{
	calib_status_t st;
	const calib_area_t *a = openplc_calib_get(&st);
	(void)openplc_calib_get(&st);   /* a second call must not log again */
	check(st == want, name);
	int nominal = 1;
	for (int i = 0; i < 4; i++) {
		nominal &= (a->ch[i].gain == 1.0f) && (a->ch[i].offset == 0.0f);
	}
	check(nominal, "every channel falls back to gain 1, offset 0");
	check(near(openplc_ai1_mv_from_raw(2048, &a->ch[CALIB_CH_AI1]), nominal_ai1(2048)), "AI1 reads the nominal conversion");
	check(log_lines == 1, "exactly one log line");
}

int main(int argc, char **argv)
{
	const char *c = (argc > 1) ? argv[1] : "";
	build_valid_area();
	printf("T3-07 %s\n", c);

	if (strcmp(c, "ok") == 0) {
		expect_calibrated();
	} else if (strcmp(c, "blank") == 0) {
		memset(test_calib_area, 0xFF, sizeof(test_calib_area));
		expect_nominal(CALIB_BLANK, "status is CALIB_BLANK (magic wrong)");
	} else if (strcmp(c, "corrupt") == 0) {
		test_calib_area[21] ^= 0x01U;           /* a coefficient byte; CRC now wrong */
		expect_nominal(CALIB_CORRUPT, "status is CALIB_CORRUPT (CRC wrong)");
	} else if (strcmp(c, "other") == 0) {
		put32(8, test_uid[0] ^ 1U);
		put32(52, crc32(test_calib_area, 52));  /* valid CRC, another chip's UID */
		expect_nominal(CALIB_OTHER_BOARD, "status is CALIB_OTHER_BOARD (UID differs)");
	} else {
		printf("usage: calib_test ok|blank|corrupt|other\n");
		return 2;
	}
	printf("%s\n", failures ? "FAILED" : "all checks passed");
	return failures ? 1 : 0;
}
