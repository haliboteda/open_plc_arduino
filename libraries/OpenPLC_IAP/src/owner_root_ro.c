/*
 * owner_root_ro.c -- see owner_root_ro.h.
 */

#include "owner_root_ro.h"
#include "fw_verify.h"
#include "iap_keyderive.h"
#include "sha256.h"
#include <string.h>
#include <stdbool.h>

/* Must match open_plc_cube_ide/IAPServer/owner_slot.h exactly -- see the note
 * at the top of owner_root_ro.h. */
/* Overridable only so a host test can point this at a RAM buffer instead of
 * memory-mapped flash; the firmware build never defines it. See T2-21 in
 * $PROD/docs/modules/M2-ownership.md. */
#ifndef OWNER_SLOT_BASE
#define OWNER_SLOT_BASE          0x081E2000UL
#endif
#define OWNER_RECORD_SIZE        160U
#define OWNER_SLOT_MAX_RECORDS   32U
#define OWNER_SEG_O_BASE         OWNER_SLOT_BASE
#define OWNER_SEG_O_SIZE         (OWNER_RECORD_SIZE * OWNER_SLOT_MAX_RECORDS)
#define OWNER_REVOKE_REC_SIZE    32U
#define OWNER_REVOKE_MAX_RECORDS 96U
#define OWNER_SEG_R_BASE         (OWNER_SEG_O_BASE + OWNER_SEG_O_SIZE)
#define OWNER_SEG_R_SIZE         (OWNER_REVOKE_REC_SIZE * OWNER_REVOKE_MAX_RECORDS)
#define OWNER_RECORD_TYPE        'O'
#define OWNER_RECORD_TYPE_REVOKE 'R'
#define OWNER_FORMAT_VER         4U
#define OWNER_FLAG_CLEARED       0x00000001UL
#define OWNER_SIGNED_PREFIX_LEN  88U
#define OWNER_REVOKE_PREFIX_LEN  16U

typedef struct {
	uint8_t  type;
	uint8_t  reserved0;        /* was `slots` through format_ver 2 */
	uint16_t format_ver;
	uint32_t generation;
	uint32_t flags;
	uint8_t  root_pubkey[64];
	uint8_t  uid[12];
	uint8_t  prev_sig[64];
	uint8_t  reserved[8];
} owner_record_t;

/* One revoked leaf, one flash word. No signature and no generation -- see
 * owner_slot.h. */
typedef struct {
	uint8_t  type;
	uint8_t  reserved0;
	uint16_t format_ver;
	uint8_t  uid[12];
	uint8_t  leaf_prefix[OWNER_REVOKE_PREFIX_LEN];
} owner_revoke_rec_t;

_Static_assert(sizeof(owner_record_t) == OWNER_RECORD_SIZE,
		"owner_record_t must match open_plc_cube_ide/IAPServer/owner_slot.h exactly");
_Static_assert(sizeof(owner_revoke_rec_t) == OWNER_REVOKE_REC_SIZE,
		"owner_revoke_rec_t must match open_plc_cube_ide/IAPServer/owner_slot.h exactly");
_Static_assert(OWNER_SEG_O_SIZE + OWNER_SEG_R_SIZE == 8U * 1024U,
		"the 'O' and 'R' segments must tile the 8K area exactly");

/* Every 'R' record written for this board. Mirror of the same array in
 * owner_slot.c -- see that file for why revocation is cumulative rather than
 * "only the last record counts". */
static const owner_revoke_rec_t *s_revoke_records[OWNER_REVOKE_MAX_RECORDS];
static uint32_t s_revoke_count;
static bool s_scanned;

static const owner_record_t *record_at(uint32_t index)
{
	return (const owner_record_t *)(OWNER_SEG_O_BASE + (index * OWNER_RECORD_SIZE));
}

static const owner_revoke_rec_t *revoke_at(uint32_t index)
{
	return (const owner_revoke_rec_t *)(OWNER_SEG_R_BASE
			+ (index * OWNER_REVOKE_REC_SIZE));
}

static bool record_is_structurally_valid(const owner_record_t *r)
{
	return (r->type == (uint8_t)OWNER_RECORD_TYPE) &&
			(r->format_ver == (uint16_t)OWNER_FORMAT_VER);
}

static bool revoke_is_structurally_valid(const owner_revoke_rec_t *r)
{
	return (r->type == (uint8_t)OWNER_RECORD_TYPE_REVOKE) &&
			(r->format_ver == (uint16_t)OWNER_FORMAT_VER);
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
 * public entry points below share one scan instead of each re-walking 32
 * records on every call. */
static void resolve_chain(void)
{
	const owner_record_t *current = NULL;
	uint32_t last_gen = 0U;
	uint32_t i;
	bool first = true;
	bool prev_cleared = false;
	uint8_t my_uid[IAP_MACHINE_ID_SIZE];

	if (s_scanned) {
		return;
	}
	s_scanned = true;

	iap_keyderive_get_machine_id(my_uid);

	for (;;) {
		const owner_record_t *next = NULL;

		for (i = 0U; i < OWNER_SLOT_MAX_RECORDS; i++) {
			const owner_record_t *r = record_at(i);

			if (!record_is_structurally_valid(r)) {
				continue;
			}
			/* 'R' records are not links in the chain -- collected separately
			 * below. Mirrors owner_slot.c. */
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

		bool cleared = ((next->flags & OWNER_FLAG_CLEARED) != 0UL);

		if (!cleared && (memcmp(next->uid, my_uid, sizeof(my_uid)) != 0)) {
			break;
		}

		if (sig_is_absent(next)) {
			if (!first && !cleared && !prev_cleared) {
				break;
			}
		} else {
			uint8_t digest[SHA256_DIGEST_SIZE];

			/* No root in force before it: nothing to verify against. */
			if ((current == NULL) || prev_cleared) {
				break;
			}
			sha256((const uint8_t *)next, OWNER_SIGNED_PREFIX_LEN, digest);
			if (!fw_verify_signature_with_key(current->root_pubkey, digest,
					next->prev_sig)) {
				break;
			}
		}

		current = next;
		last_gen = next->generation;
		prev_cleared = cleared;
		first = false;
	}

	s_effective = current;
	s_effective_cleared = (current != NULL) && ((current->flags & OWNER_FLAG_CLEARED) != 0UL);

	/* Revocations are a table, not a step in the ownership history: structure
	 * and uid only, no signature and no generation. Mirrors owner_slot.c --
	 * the two must agree or the app and the bootloader would disagree about
	 * which leaves are still good. */
	s_revoke_count = 0U;
	for (i = 0U; i < OWNER_REVOKE_MAX_RECORDS; i++) {
		const owner_revoke_rec_t *r = revoke_at(i);

		if (!revoke_is_structurally_valid(r)) {
			continue;
		}
		if (memcmp(r->uid, my_uid, sizeof(my_uid)) != 0) {
			continue;
		}
		s_revoke_records[s_revoke_count++] = r;
	}
}

bool owner_root_ro_get(uint8_t out[64])
{
	resolve_chain();

	if ((s_effective == NULL) || s_effective_cleared) {
		return false;
	}
	memcpy(out, s_effective->root_pubkey, 64U);
	return true;
}

bool owner_root_ro_is_revoked(const uint8_t leaf_pubkey[64])
{
	uint8_t root[64];
	uint32_t i;

	if (!owner_root_ro_get(root)) {
		return false;
	}

	for (i = 0U; i < s_revoke_count; i++) {
		const uint8_t *entry = s_revoke_records[i]->leaf_prefix;

		/* R4: the root in force can never revoke itself. */
		if (memcmp(entry, root, OWNER_REVOKE_PREFIX_LEN) == 0) {
			continue;
		}
		if (memcmp(entry, leaf_pubkey, OWNER_REVOKE_PREFIX_LEN) == 0) {
			return true;
		}
	}
	return false;
}
