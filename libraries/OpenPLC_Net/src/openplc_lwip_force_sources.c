/*
 * OpenPLC fallback lwIP build unit.
 *
 * Some Arduino build environments for this core resolve lwIP headers via
 * include paths but fail to compile/link STM32duino_LwIP library sources,
 * resulting in undefined references at link stage.
 *
 * To keep OpenPLC_Net self-contained in those environments, compile the
 * required lwIP implementation files directly as part of this library.
 *
 * Define OPENPLC_USE_EXTERNAL_LWIP=1 to disable this fallback when your
 * toolchain correctly links STM32duino_LwIP as a separate Arduino library.
 */

#ifndef OPENPLC_USE_EXTERNAL_LWIP
#define OPENPLC_USE_EXTERNAL_LWIP 0
#endif

#if !OPENPLC_USE_EXTERNAL_LWIP

/* lwIP core */
#include "../../STM32duino_LwIP/src/core/def.c"
#include "../../STM32duino_LwIP/src/core/dns.c"
#include "../../STM32duino_LwIP/src/core/init.c"
#include "../../STM32duino_LwIP/src/core/inet_chksum.c"
#include "../../STM32duino_LwIP/src/core/ip.c"
#include "../../STM32duino_LwIP/src/core/mem.c"
#include "../../STM32duino_LwIP/src/core/memp.c"
#include "../../STM32duino_LwIP/src/core/netif.c"
#include "../../STM32duino_LwIP/src/core/pbuf.c"
#include "../../STM32duino_LwIP/src/core/raw.c"
#include "../../STM32duino_LwIP/src/core/stats.c"
#include "../../STM32duino_LwIP/src/core/sys.c"
#include "../../STM32duino_LwIP/src/core/tcp.c"
#include "../../STM32duino_LwIP/src/core/tcp_in.c"
#include "../../STM32duino_LwIP/src/core/tcp_out.c"
#include "../../STM32duino_LwIP/src/core/timeouts.c"
#include "../../STM32duino_LwIP/src/core/udp.c"

/* lwIP IPv4 */
#include "../../STM32duino_LwIP/src/core/ipv4/autoip.c"
#include "../../STM32duino_LwIP/src/core/ipv4/dhcp.c"
#include "../../STM32duino_LwIP/src/core/ipv4/etharp.c"
#include "../../STM32duino_LwIP/src/core/ipv4/icmp.c"
#include "../../STM32duino_LwIP/src/core/ipv4/igmp.c"
#include "../../STM32duino_LwIP/src/core/ipv4/ip4.c"
#include "../../STM32duino_LwIP/src/core/ipv4/ip4_addr.c"
#include "../../STM32duino_LwIP/src/core/ipv4/ip4_frag.c"

/* lwIP netif */
#include "../../STM32duino_LwIP/src/netif/ethernet.c"

#endif /* !OPENPLC_USE_EXTERNAL_LWIP */
