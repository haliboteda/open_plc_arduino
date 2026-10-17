/*
 * fw_meta_ro.c -- see fw_meta_ro.h.
 */

#include "fw_meta_ro.h"
#include "iap_cert.h"
#include <string.h>

/* Must match open_plc_cube_ide/IAPServer/bootloader_state.c (IAP_META_BASE,
 * IAP_MARKER_ADDR, IAP_META_SLOT_SIZE, IAP_METADATA_SLOTS, IAP_REC_METADATA).
 * Layout: $PROD/docs/modules/M1/SECTOR-15.md. */
#define FW_META_BASE        0x081E4000UL   /* end of the root area */
#define FW_META_END         0x081FFFE0UL   /* the completion marker */
#define FW_META_SLOT_SIZE   32U
#define FW_META_REC_SLOTS   7U
#define FW_META_REC_TYPE    0x4DU          /* 'M' */
#define FW_META_SLOT_COUNT  ((FW_META_END - FW_META_BASE) / FW_META_SLOT_SIZE)

/* iap_meta_rec_t in bootloader_state.c with iap_fw_metadata_t inlined. */
typedef struct {
	uint8_t    type;
	uint8_t    slots;
	uint16_t   reserved0;
	uint32_t   reserved1;
	uint32_t   app_size;
	uint8_t    signature[64];
	iap_cert_t cert;
	uint8_t    reserved[20];
} fw_meta_rec_t;

_Static_assert(sizeof(fw_meta_rec_t) == (FW_META_REC_SLOTS * FW_META_SLOT_SIZE),
		"fw_meta_rec_t must match iap_meta_rec_t in open_plc_cube_ide/IAPServer/bootloader_state.c");

bool fw_meta_ro_installed_leaf(uint8_t out[64])
{
	const fw_meta_rec_t *last = NULL;
	uint32_t i = 0U;

	/* Same walk as meta_scan(): the last 'M' record wins; a blank or
	 * unrecognised slot ends the scan. */
	while (i < FW_META_SLOT_COUNT) {
		const fw_meta_rec_t *r = (const fw_meta_rec_t *)(FW_META_BASE + (i * FW_META_SLOT_SIZE));

		if ((r->type != FW_META_REC_TYPE) || (r->slots != FW_META_REC_SLOTS)
				|| ((i + FW_META_REC_SLOTS) > FW_META_SLOT_COUNT)) {
			break;
		}
		last = r;
		i += FW_META_REC_SLOTS;
	}
	if ((last == NULL) || (last->app_size == 0U)) {
		return false;
	}
	memcpy(out, last->cert.leaf_pubkey, 64U);
	return true;
}
