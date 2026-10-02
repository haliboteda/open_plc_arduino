/*
 * openplc_calib.h -- reads the per-board AI/AO calibration the fixture wrote.
 *
 * Mirror of the bootloader's IAPServer/calib_area.h, which owns the format;
 * P2 checks they agree. Byte table and meaning of the coefficients:
 * $PROD/docs/modules/M1/SECTOR-15.md, "校准值区的格式".
 */

#ifndef OPENPLC_CALIB_H_
#define OPENPLC_CALIB_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Overridable so the host test (T3-07) can point it at a RAM buffer. */
#ifndef CALIB_AREA_ADDR
#define CALIB_AREA_ADDR   0x081E0000UL
#endif
#define CALIB_AREA_SIZE   (8U * 1024U)

#define CALIB_MAGIC       0x4C41434FUL   /* "OCAL" */
#define CALIB_VERSION     1U
#define CALIB_CHANNELS    4U

typedef enum {
	CALIB_CH_AI1 = 0,   /* mV */
	CALIB_CH_AI2,       /* mA */
	CALIB_CH_AO1,       /* mA */
	CALIB_CH_AO2        /* mA */
} calib_channel_id_t;

/* measured ~= gain * nominal + offset */
typedef struct {
	float gain;
	float offset;
} calib_channel_t;

typedef struct {
	uint32_t        magic;
	uint16_t        version;
	uint16_t        channels;
	uint32_t        uid[3];              /* w0 w1 w2 */
	calib_channel_t ch[CALIB_CHANNELS];
	uint32_t        crc32;               /* CRC-32 (IEEE) of every byte before it */
} calib_area_t;

_Static_assert(sizeof(calib_area_t) == 56U, "calib_area_t is the on-flash format");

typedef enum {
	CALIB_OK = 0,
	CALIB_BLANK,         /* magic wrong: never written */
	CALIB_CORRUPT,       /* CRC or version wrong */
	CALIB_OTHER_BOARD    /* UID is not this chip's */
} calib_status_t;

/* Copies the area into *out when it is valid for this board. On anything but
 * CALIB_OK, *out holds gain 1 / offset 0 for every channel, i.e. nominal. */
calib_status_t openplc_calib_load(calib_area_t *out);

/* The same checks on an area already in memory, against the given chip UID
 * (w0 w1 w2). openplc_calib_load() is this applied to the flash copy. */
calib_status_t openplc_calib_check(const calib_area_t *a, const uint32_t uid[3],
                                   calib_area_t *out);

#ifdef __cplusplus
}
#endif

#endif /* OPENPLC_CALIB_H_ */
