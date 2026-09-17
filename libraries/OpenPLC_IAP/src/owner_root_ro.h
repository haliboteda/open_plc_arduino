/*
 * owner_root_ro.h -- read-only mirror of the owner-record chain resolution in
 * open_plc_cube_ide/IAPServer/owner_slot.c, for code that needs to know which
 * root this board currently trusts but has no business writing that area.
 *
 * Added 2026-09-04 alongside the session-auth switch to ECDSA-via-certificate
 * (see $PROD/docs/modules/M2-ownership.md): the app needs a trusted root to check a
 * certificate against for `openplc_server_reboot`, and the only place that
 * answer lives is the owner-record area in the bootloader's own flash sector
 * -- there was no channel handing it to the app before this. The area is
 * memory-mapped flash on the same chip the app is running on (no WRP/RDP), so
 * this reads it directly rather than inventing a new communication path.
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

#ifdef __cplusplus
extern "C" {
#endif

/* The root this board currently trusts: the last verified link in the
 * on-flash owner chain, or the compiled-in fw_public_key when the area is
 * empty or resolves to nothing valid. Writes 64 bytes to out. */
void owner_root_ro_get(uint8_t out[64]);

#ifdef __cplusplus
}
#endif

#endif /* OPENPLC_IAP_OWNER_ROOT_RO_H_ */
