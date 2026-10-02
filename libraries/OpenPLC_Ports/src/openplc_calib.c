#include "openplc_calib.h"
#include "stm32_def.h"
#include <stddef.h>
#include <string.h>

/* Reflected CRC-32, polynomial 0xEDB88320: the same as zlib and Go's
 * crc32.ChecksumIEEE, which the fixture uses to write it. */
static uint32_t crc32_ieee(const uint8_t *p, uint32_t len)
{
	uint32_t crc = 0xFFFFFFFFUL;

	while (len-- > 0U) {
		crc ^= *p++;
		for (int i = 0; i < 8; i++) {
			crc = (crc >> 1) ^ (0xEDB88320UL & (0U - (crc & 1U)));
		}
	}
	return ~crc;
}

static void set_nominal(calib_area_t *out)
{
	memset(out, 0, sizeof(*out));
	for (uint32_t i = 0U; i < CALIB_CHANNELS; i++) {
		out->ch[i].gain = 1.0f;
		out->ch[i].offset = 0.0f;
	}
}

calib_status_t openplc_calib_check(const calib_area_t *a, const uint32_t uid[3],
                                   calib_area_t *out)
{
	set_nominal(out);

	if (a->magic != CALIB_MAGIC) {
		return CALIB_BLANK;
	}
	if ((a->version != CALIB_VERSION) || (a->channels != CALIB_CHANNELS) ||
	    (crc32_ieee((const uint8_t *)a, offsetof(calib_area_t, crc32)) != a->crc32)) {
		return CALIB_CORRUPT;
	}
	if ((a->uid[0] != uid[0]) || (a->uid[1] != uid[1]) || (a->uid[2] != uid[2])) {
		return CALIB_OTHER_BOARD;
	}
	*out = *a;
	return CALIB_OK;
}

calib_status_t openplc_calib_load(calib_area_t *out)
{
	calib_area_t a;
	const uint32_t uid[3] = {HAL_GetUIDw0(), HAL_GetUIDw1(), HAL_GetUIDw2()};

	/* Copied first: the flash word may be unaligned for a direct struct read. */
	memcpy(&a, (const void *)CALIB_AREA_ADDR, sizeof(a));
	return openplc_calib_check(&a, uid, out);
}
