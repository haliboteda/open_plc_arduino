/*
 * owner_root_ro.h -- read-only mirror of the owner-record chain resolution in
 * open_plc_cube_ide/IAPServer/owner_slot.c, for code that needs to know which
 * root this board currently trusts but has no business writing that area.
 *
 * The app needs the trusted root to check the certificate on
 * `openplc_server_reboot` ($PROD/docs/modules/M2-ownership.md). The root area
 * in sector 15 is memory-mapped flash on the same chip, so this reads it
 * directly.
 *
 * Deliberately read-only and deliberately a SEPARATE module from
 * owner_slot.c, not a shared library: only the bootloader is allowed to
 * append records (owner_slot_claim/set_owner/factory_reset all require
 * physical presence or a signature check this module has no reason to also
 * be capable of), and a build that can never write flash here cannot be
 * talked into becoming one.
 *
 * Must match, byte-for-byte, the record layout and resolution logic in:
 *   - open_plc_cube_ide/IAPServer/owner_slot.h/.c
 * Any change to the on-flash format has to land in both, or this side will
 * either reject valid records or -- worse -- resolve to the wrong root.
 */

#ifndef OPENPLC_IAP_OWNER_ROOT_RO_H_
#define OPENPLC_IAP_OWNER_ROOT_RO_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The root this board currently trusts: the last verified link in the
 * on-flash owner chain. Writes 64 bytes to out and returns true; returns
 * false when the board has no root (decision 72), and nothing verifies. */
bool owner_root_ro_get(uint8_t out[64]);

/* Generation of the record in force, 0 when there is none. Mirror of
 * owner_slot_generation() in open_plc_cube_ide/IAPServer/owner_slot.c. */
uint32_t owner_root_ro_generation(void);

/*
 * Has this leaf been revoked by a signed 'R' record in the trusted chain?
 * Mirror of owner_slot_is_revoked() in open_plc_cube_ide/IAPServer/owner_slot.c
 * -- same comparison (first 16 bytes only), same R4 exemption (an entry
 * naming the root currently in force is skipped, never honoured).
 */
bool owner_root_ro_is_revoked(const uint8_t leaf_pubkey[64]);

#ifdef __cplusplus
}
#endif

#endif /* OPENPLC_IAP_OWNER_ROOT_RO_H_ */
