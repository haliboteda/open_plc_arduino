/*
 * fw_pubkey.c -- see fw_pubkey.h. Mirrors
 * open_plc_cube_ide/IAPServer/fw_pubkey.c. Rotate with
 * IAPServer/keys/rotate_keys.sh.
 */

#include "fw_pubkey.h"

const uint8_t fw_public_key[64] = {
#include "keys/fw_pubkey.inc"
};
