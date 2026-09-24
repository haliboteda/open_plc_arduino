/*
 * openplc_rng.h
 *
 * The RNG peripheral, shared by the IAP challenge nonce (OpenPLC_IAP) and lwIP.
 * See $PROD/docs/tables/DECISIONS.md, decisions 66 and 67.
 */

#ifndef OPENPLC_RNG_H_
#define OPENPLC_RNG_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Fills `words` from the RNG, bringing it up on first use. False if the RNG
 * could not be started or any word failed; `words` is then not to be used. */
bool openplc_rng_words(uint32_t *words, uint32_t n);

/* Seeds rand(), which is what LWIP_RAND() calls for DHCP transaction IDs and
 * ephemeral ports. Called before lwip_init(). If the RNG fails, rand() is left
 * as it was. */
void openplc_rng_seed_rand(void);

/* Initial sequence number for a new TCP connection, via LWIP_HOOK_TCP_ISN in
 * lwipopts_default.h. Falls back to rand() if the RNG fails. */
uint32_t openplc_rng_tcp_isn(void);

#ifdef __cplusplus
}
#endif

#endif /* OPENPLC_RNG_H_ */
