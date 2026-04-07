#include "knx_ip.h"
#include "knx_config.h"
#include "knx_telegram.h"

extern "C" {
#include "lwip/udp.h"
#include "lwip/pbuf.h"
#include "lwip/ip_addr.h"
#include "lwip/igmp.h"
#include "lwip/inet.h"
}

/* -------------------------------------------------------------------------
 * KNXnet/IP constants
 * ---------------------------------------------------------------------- */

/* KNXnet/IP header size (bytes) */
#define KNXIP_HEADER_LEN        6u

/* Service type identifiers (big-endian 2-byte values) */
#define KNXIP_SVC_ROUTING_IND   0x0530u

/* Protocol version */
#define KNXIP_VERSION           0x10u

/* Maximum cEMI frame size we will send/accept */
#define KNXIP_CEMI_MAX_LEN      (8u + KNX_APDU_BUF_LEN)

/* Maximum UDP payload */
#define KNXIP_UDP_MAX_LEN       (KNXIP_HEADER_LEN + KNXIP_CEMI_MAX_LEN)

/* -------------------------------------------------------------------------
 * Internal state
 * ---------------------------------------------------------------------- */

static struct udp_pcb *s_pcb    = NULL;
static knx_ip_rx_cb_t  s_rx_cb  = NULL;
static bool            s_ready  = false;
static ip4_addr_t      s_mcast_addr;

/* -------------------------------------------------------------------------
 * Build / parse KNXnet/IP header
 * ---------------------------------------------------------------------- */

static void knxip_write_header(uint8_t *buf, uint16_t svc_type, uint16_t total_len)
{
    buf[0] = 0x06u;                         /* Header length */
    buf[1] = 0x00u;
    buf[2] = KNXIP_VERSION;                 /* Version 0x10 */
    buf[3] = 0x00u;
    buf[4] = (uint8_t)(svc_type >> 8);      /* Service type high byte */
    buf[5] = (uint8_t)(svc_type & 0xFFu);  /* Service type low byte */
    buf[6] = (uint8_t)(total_len >> 8);     /* Total length high byte */
    buf[7] = (uint8_t)(total_len & 0xFFu); /* Total length low byte */
}

/* Returns the service type from a received KNXnet/IP header, or 0 on error. */
static uint16_t knxip_parse_header(const uint8_t *buf, uint8_t len,
                                   uint8_t *out_svc_data_offset,
                                   uint16_t *out_svc_data_len)
{
    if (len < KNXIP_HEADER_LEN + 2u) {
        return 0u;
    }
    uint8_t  hdr_len = buf[0];
    uint8_t  version = buf[2];
    if (hdr_len < KNXIP_HEADER_LEN || version != KNXIP_VERSION) {
        return 0u;
    }
    uint16_t total_len = (uint16_t)(((uint16_t)buf[6] << 8) | buf[7]);
    if (total_len < hdr_len || total_len > len) {
        return 0u;
    }
    uint16_t svc_type = (uint16_t)(((uint16_t)buf[4] << 8) | buf[5]);
    *out_svc_data_offset = hdr_len;
    *out_svc_data_len    = (uint16_t)(total_len - hdr_len);
    return svc_type;
}

/* -------------------------------------------------------------------------
 * LwIP UDP receive callback
 * ---------------------------------------------------------------------- */

static void knx_ip_udp_recv(void *arg,
                             struct udp_pcb *pcb,
                             struct pbuf *p,
                             const ip_addr_t *addr,
                             u16_t port)
{
    (void)arg; (void)pcb; (void)addr; (void)port;

    if (p == NULL) {
        return;
    }

    /* Copy pbuf payload to a flat buffer */
    uint16_t plen = (uint16_t)(p->tot_len < KNXIP_UDP_MAX_LEN
                                ? p->tot_len : KNXIP_UDP_MAX_LEN);
    uint8_t  buf[KNXIP_UDP_MAX_LEN];
    pbuf_copy_partial(p, buf, plen, 0u);
    pbuf_free(p);

    /* Parse KNXnet/IP header */
    uint8_t  svc_off  = 0u;
    uint16_t svc_len  = 0u;
    uint16_t svc_type = knxip_parse_header(buf, (uint8_t)plen, &svc_off, &svc_len);

    if (svc_type != KNXIP_SVC_ROUTING_IND || svc_len == 0u) {
        return;
    }

    /* Decode cEMI frame */
    KnxTelegram tg;
    if (knxTelegram_fromCEMI(&tg, &buf[svc_off], (uint8_t)svc_len)) {
        if (s_rx_cb != NULL) {
            s_rx_cb(&tg);
        }
    }
}

/* -------------------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------------- */

bool knx_ip_init(void)
{
    if (s_ready) {
        return true;
    }

    /* Parse the multicast address string */
    if (!ip4addr_aton(KNX_IP_MCAST_STR, &s_mcast_addr)) {
        return false;
    }

    /* Create UDP PCB */
    s_pcb = udp_new();
    if (s_pcb == NULL) {
        return false;
    }

    /* Allow multiple sockets on the same port */
    ip_set_option(s_pcb, SOF_REUSEADDR);

    /* Bind to any local address, port 3671 */
    err_t err = udp_bind(s_pcb, IP4_ADDR_ANY, (u16_t)KNX_IP_PORT);
    if (err != ERR_OK) {
        udp_remove(s_pcb);
        s_pcb = NULL;
        return false;
    }

    /* Join the KNX IP multicast group */
#if LWIP_IGMP
    err = igmp_joingroup(IP4_ADDR_ANY, &s_mcast_addr);
    if (err != ERR_OK) {
        /* Non-fatal: device can still send but may not receive multicast
         * from other devices on a managed switch. Log and continue. */
    }
#endif

    /* Register receive callback */
    udp_recv(s_pcb, knx_ip_udp_recv, NULL);

    s_ready = true;
    return true;
}

void knx_ip_set_rx_callback(knx_ip_rx_cb_t cb)
{
    s_rx_cb = cb;
}

bool knx_ip_send(const KnxTelegram *tg)
{
    if (!s_ready || s_pcb == NULL) {
        return false;
    }

    /* Build cEMI frame */
    uint8_t cemi[KNXIP_CEMI_MAX_LEN];
    uint8_t cemi_len = knxTelegram_toCEMI(tg, cemi, (uint8_t)sizeof(cemi),
                                           KNX_CEMI_L_DATA_REQ);
    if (cemi_len == 0u) {
        return false;
    }

    /* Total packet = 8-byte KNXnet/IP header + cEMI */
    uint16_t total_len = (uint16_t)(KNXIP_HEADER_LEN + 2u + cemi_len);
    /* (The standard header is 6 bytes but we write the 2-byte total_len field too,
     *  making the physical header 8 bytes: 6 fixed + 2 length. Re-check below.) */

    /* Correct total_len calculation:
     * KNXnet/IP header = 6 bytes (bytes 0-5 as defined by spec)
     * BUT total_len field (at bytes 6-7) is part of the same 8-byte structure.
     * Spec says total_len = count of all bytes in the packet including header.
     * total_len = 8 (full header with length field) + cemi_len */
    total_len = (uint16_t)(8u + cemi_len);

    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, total_len, PBUF_RAM);
    if (p == NULL) {
        return false;
    }

    uint8_t *payload = (uint8_t *)p->payload;
    /* Write 6-byte spec header + 2-byte total length = 8 bytes */
    payload[0] = 0x06u;                                /* Header length = 6 */
    payload[1] = 0x00u;
    payload[2] = KNXIP_VERSION;
    payload[3] = 0x00u;
    payload[4] = (uint8_t)(KNXIP_SVC_ROUTING_IND >> 8);
    payload[5] = (uint8_t)(KNXIP_SVC_ROUTING_IND & 0xFFu);
    payload[6] = (uint8_t)(total_len >> 8);
    payload[7] = (uint8_t)(total_len & 0xFFu);
    memcpy(&payload[8], cemi, cemi_len);

    ip_addr_t dst;
    ip4_addr_copy(*ip_2_ip4(&dst), s_mcast_addr);
    IP_SET_TYPE_VAL(dst, IPADDR_TYPE_V4);

    err_t err = udp_sendto(s_pcb, p, &dst, (u16_t)KNX_IP_PORT);
    pbuf_free(p);
    return (err == ERR_OK);
}

bool knx_ip_ready(void)
{
    return s_ready;
}
