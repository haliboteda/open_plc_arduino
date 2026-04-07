#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "knx_telegram.h"

/*
 * KNXnet/IP transport layer — Routing mode (multicast).
 *
 * KNXnet/IP Routing (KNX spec Part 3.8.7) uses:
 *   Destination IP : 224.0.23.12  (KNX_IP_MCAST_STR in knx_config.h)
 *   Port           : 3671         (KNX_IP_PORT)
 *   Protocol       : UDP multicast, connectionless
 *
 * This is the simplest KNXnet/IP service:
 *   - No connection handshake required (unlike KNXnet/IP Tunneling)
 *   - Every device on the network can receive and send routing indications
 *   - Suitable for OpenPLC acting as a KNX IP router / IP-enabled device
 *
 * Dependency:
 *   Requires OpenPLC_Net (LwIP) to be initialized before calling knx_ip_init().
 *   Call openplc_net_init() and wait for a valid IP before calling knx_ip_init().
 *
 * KNXnet/IP header wire format (6 bytes, always prepended to service data):
 *   Byte 0-1 : Header length = 0x06
 *   Byte 2-3 : Protocol version = 0x10 (KNXnet/IP v1.0)
 *   Byte 4-5 : Service type identifier (0x0530 = Routing Indication)
 *   Byte 6-7 : Total packet length (big-endian, header + service data)
 *
 * Routing Indication packet = KNXnet/IP header (6 bytes) + cEMI frame
 */

typedef void (*knx_ip_rx_cb_t)(const KnxTelegram *tg);

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize the KNXnet/IP routing transport.
 * Opens a UDP PCB on port 3671 and joins the KNX multicast group.
 * Returns true on success. */
bool knx_ip_init(void);

/* Register a callback invoked for each received routing indication. */
void knx_ip_set_rx_callback(knx_ip_rx_cb_t cb);

/* Send a telegram as a KNXnet/IP Routing Indication to the multicast group.
 * Returns true if the datagram was submitted to LwIP successfully. */
bool knx_ip_send(const KnxTelegram *tg);

/* Return true if the IP transport has been successfully initialized. */
bool knx_ip_ready(void);

#ifdef __cplusplus
}
#endif
