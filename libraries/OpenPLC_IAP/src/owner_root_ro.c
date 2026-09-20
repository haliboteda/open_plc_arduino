/*
 * owner_root_ro.c -- see owner_root_ro.h.
 */

#include "owner_root_ro.h"
#include "fw_pubkey.h"
#include "fw_verify.h"
#include "iap_keyderive.h"
#include "sha256.h"
#include <string.h>
#include <stdbool.h>

/* Must match open_plc_cube_ide/IAPServer/owner_slot.h exactly -- see the note
 * at the top of owner_root_ro.h. */
#define OWNER_SLOT_BASE          0x0801E000UL
#define OWNER_RECORD_SIZE        160U
#define OWNER_SLOT_MAX_RECORDS   51U
#define OWNER_RECORD_TYPE        'O'
#define OWNER_RECORD_TYPE_REVOKE 'R'
#define OWNER_FORMAT_VER         3U
#define OWNER_FLAG_CLEARED       0x00000001UL
#define OWNER_SIGNED_PREFIX_LEN  88U
#define OWNER_REVOKE_SLOTS       4U
#define OWNER_REVOKE_PREFIX_LEN  16U

typedef struct {
	uint8_t  type;
	uint8_t  reserved0;        /* was `slots` through format_ver 2 */
	uint16_t format_ver;
	uint32_t generation;
	uint32_t flags;
	union {
		uint8_t root_pubkey[64];
		uint8_t revoked[OWNER_REVOKE_SLOTS][OWNER_REVOKE_PREFIX_LEN];
	};
	uint8_t  uid[12];
	uint8_t  prev_sig[64];
	uint8_t  reserved[8];
} owner_record_t;

_Static_assert(sizeof(owner_record_t) == OWNER_RECORD_SIZE,
		"owner_record_t must match open_plc_cube_ide/IAPServer/owner_slot.h exactly");

/* Every 'R' record the walk below accepted into the trusted chain. Mirror of
 * the same array in owner_slot.c -- see that file for why revocation is
 * cumulative rather than "only the last record counts". */
static const owner_record_t *s_revoke_records[OWNER_SLOT_MAX_RECORDS];
static uint32_t s_revoke_count;
static bool s_scanned;

static const owner_record_t *record_at(uint32_t index)
{
	return (const owner_record_t *)(OWNER_SLOT_BASE + (index * OWNER_RECORD_SIZE));
}

static bool record_is_structurally_valid(const owner_record_t *r)
{
	if ((r->type != (uint8_t)OWNER_RECORD_TYPE) &&
			(r->type != (uint8_t)OWNER_RECORD_TYPE_REVOKE)) {
		return false;
	}
	if (r->format_ver != (uint16_t)OWNER_FORMAT_VER) {
		return false;
	}
	return true;
}

static bool payload_slice_is_zero(const uint8_t *p, uint32_t len)
{
	uint32_t i;

	for (i = 0U; i < len; i++) {
		if (p[i] != 0U) {
			return false;
		}
	}
	return true;
}

static bool sig_is_absent(const owner_record_t *r)
{
	uint32_t i;

	for (i = 0U; i < sizeof(r->prev_sig); i++) {
		if (r->prev_sig[i] != 0U) {
			return false;
		}
	}
	return true;
}

static const owner_record_t *s_effective;
static bool s_effective_cleared;

/* Same algorithm as resolve_chain() in owner_slot.c -- see that file for the
 * full reasoning (oldest-first walk, TOFU/cleared exceptions, a failing link
 * stops the walk rather than being skipped, uid binds a record to this
 * board, 'R' records never get a TOFU pass and never change the effective
 * owner). This copy only needs the end result, not the diagnostic counters
 * owner_slot.c keeps for owner_slot_report().
 *
 * Cached for the lifetime of this boot: the app can never write this area
 * (only the bootloader can, and that requires a reboot into it first), so the
 * flash content this reads cannot change out from under a running app. Both
 * public entry points below share one scan instead of each re-walking 51
 * records on every call. */
static void resolve_chain(void)
{
	const owner_record_t *current = NULL;
	uint32_t last_gen = 0U;
	bool first = true;
	bool prev_cleared = false;
	uint8_t my_uid[IAP_MACHINE_ID_SIZE];

	if (s_scanned) {
		return;
	}
	s_scanned = true;
	s_revoke_count = 0U;

	iap_keyderive_get_machine_id(my_uid);

	for (;;) {
		const owner_record_t *next = NULL;
		uint32_t i;

		for (i = 0U; i < OWNER_SLOT_MAX_RECORDS; i++) {
			const owner_record_t *r = record_at(i);

			if (!record_is_structurally_valid(r)) {
				continue;
			}
			if (!first && (r->generation <= last_gen)) {
				continue;
			}
			if ((next == NULL) || (r->generation < next->generation)) {
				next = r;
			}
		}
		if (next == NULL) {
			break;
		}

		bool is_revoke = (next->type == (uint8_t)OWNER_RECORD_TYPE_REVOKE);
		bool cleared = !is_revoke && ((next->flags & OWNER_FLAG_CLEARED) != 0UL);

		if (!cleared && (memcmp(next->uid, my_uid, sizeof(my_uid)) != 0)) {
			break;
		}

		if (sig_is_absent(next)) {
			if (is_revoke || (!first && !cleared && !prev_cleared)) {
				break;
			}
		} else {
			uint8_t digest[SHA256_DIGEST_SIZE];
			const uint8_t *signer = ((current != NULL) && !prev_cleared)
					? current->root_pubkey : fw_public_key;

			sha256((const uint8_t *)next, OWNER_SIGNED_PREFIX_LEN, digest);
			if (!fw_verify_signature_with_key(signer, digest, next->prev_sig)) {
				break;
			}
		}

		if (is_revoke) {
			if (s_revoke_count < OWNER_SLOT_MAX_RECORDS) {
				s_revoke_records[s_revoke_count++] = next;
			}
		} else {
			current = next;
		}
		last_gen = next->generation;
		prev_cleared = cleared;
		first = false;
	}

	s_effective = current;
	s_effective_cleared = (current != NULL) && ((current->flags & OWNER_FLAG_CLEARED) != 0UL);
}

void owner_root_ro_get(uint8_t out[64])
{
	resolve_chain();

	if ((s_effective != NULL) && !s_effective_cleared) {
		memcpy(out, s_effective->root_pubkey, 64U);
	} else {
		memcpy(out, fw_public_key, 64U);
	}
}

bool owner_root_ro_is_revoked(const uint8_t leaf_pubkey[64])
{
	uint8_t root[64];
	uint32_t i, j;

	resolve_chain();
	owner_root_ro_get(root);

	for (i = 0U; i < s_revoke_count; i++) {
		const owner_record_t *r = s_revoke_records[i];

		for (j = 0U; j < OWNER_REVOKE_SLOTS; j++) {
			const uint8_t *entry = r->revoked[j];

			if (payload_slice_is_zero(entry, OWNER_REVOKE_PREFIX_LEN)) {
				continue;
			}
			/* R4: the root in force can never revoke itself. */
			if (memcmp(entry, root, OWNER_REVOKE_PREFIX_LEN) == 0) {
				continue;
			}
			if (memcmp(entry, leaf_pubkey, OWNER_REVOKE_PREFIX_LEN) == 0) {
				return true;
			}
		}
	}
	return false;
}
