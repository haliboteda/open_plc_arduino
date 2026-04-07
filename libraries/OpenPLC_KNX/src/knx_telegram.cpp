#include "knx_telegram.h"
#include <string.h>

/* -------------------------------------------------------------------------
 * Internal helpers
 * ---------------------------------------------------------------------- */

/* Compute the KNX standard frame checksum.
 * The checksum byte satisfies: XOR of all frame bytes (including checksum) == 0xFF.
 * Therefore: checksum = 0xFF ^ (XOR of all preceding bytes).
 */
static uint8_t compute_checksum(const uint8_t *buf, uint8_t len)
{
    uint8_t cs = 0xFFu;
    for (uint8_t i = 0u; i < len; i++) {
        cs ^= buf[i];
    }
    return cs;
}

/* Fill the APDU portion of a telegram for group communication.
 * APDU[0] = 0x00  (T_DATA_GROUP TPCI + APCI[9:8] — always 0 for group value services)
 * APDU[1] = service_apci | (short_data if data_len==1 and value fits in 6 bits)
 * APDU[2+] = remaining data bytes
 */
static void build_group_apdu(KnxTelegram *tg,
                              uint8_t service_apci,
                              const uint8_t *data, uint8_t data_len)
{
    tg->apdu[0] = 0x00u; /* TPCI for T_DATA_GROUP */

    if (data_len == 0u) {
        /* GroupValue.Read: no data payload */
        tg->apdu[1]  = service_apci;
        tg->apdu_len = 2u;
    } else if (data_len == 1u && (data[0] & 0xC0u) == 0u) {
        /* Short data: value fits in 6 bits, packed into APCI byte */
        tg->apdu[1]  = (uint8_t)(service_apci | (data[0] & 0x3Fu));
        tg->apdu_len = 2u;
    } else {
        /* Long data: APCI byte followed by data bytes */
        tg->apdu[1] = service_apci;
        uint8_t copy = (data_len < (uint8_t)(KNX_APDU_BUF_LEN - 2u))
                       ? data_len : (uint8_t)(KNX_APDU_BUF_LEN - 2u);
        memcpy(&tg->apdu[2], data, copy);
        tg->apdu_len = (uint8_t)(2u + copy);
    }
}

/* -------------------------------------------------------------------------
 * Telegram building
 * ---------------------------------------------------------------------- */

void knxTelegram_buildGroupWrite(KnxTelegram *tg,
                                 KnxIndividualAddr src,
                                 KnxGroupAddr dst,
                                 const uint8_t *data, uint8_t data_len)
{
    tg->ctrl          = KNX_CTRL_STD_NO_REPEAT;
    tg->src           = src;
    tg->dst           = dst;
    tg->dst_is_group  = true;
    tg->hop_count     = 6u;
    build_group_apdu(tg, KNX_APCI_GROUP_WRITE, data, data_len);
}

void knxTelegram_buildGroupRead(KnxTelegram *tg,
                                KnxIndividualAddr src,
                                KnxGroupAddr dst)
{
    tg->ctrl          = KNX_CTRL_STD_NO_REPEAT;
    tg->src           = src;
    tg->dst           = dst;
    tg->dst_is_group  = true;
    tg->hop_count     = 6u;
    build_group_apdu(tg, KNX_APCI_GROUP_READ, NULL, 0u);
}

void knxTelegram_buildGroupResp(KnxTelegram *tg,
                                KnxIndividualAddr src,
                                KnxGroupAddr dst,
                                const uint8_t *data, uint8_t data_len)
{
    tg->ctrl          = KNX_CTRL_STD_NO_REPEAT;
    tg->src           = src;
    tg->dst           = dst;
    tg->dst_is_group  = true;
    tg->hop_count     = 6u;
    build_group_apdu(tg, KNX_APCI_GROUP_RESP, data, data_len);
}

/* -------------------------------------------------------------------------
 * Telegram inspection
 * ---------------------------------------------------------------------- */

KnxService knxTelegram_service(const KnxTelegram *tg)
{
    if (tg->apdu_len < 2u) {
        return KNX_SVC_UNKNOWN;
    }
    switch (tg->apdu[1] & KNX_APCI_SERVICE_MASK) {
        case KNX_APCI_GROUP_READ:  return KNX_SVC_GROUP_READ;
        case KNX_APCI_GROUP_RESP:  return KNX_SVC_GROUP_RESP;
        case KNX_APCI_GROUP_WRITE: return KNX_SVC_GROUP_WRITE;
        default:                   return KNX_SVC_UNKNOWN;
    }
}

/* Internal static buffer for reconstructed short data */
static uint8_t s_short_data[1];

const uint8_t *knxTelegram_data(const KnxTelegram *tg, uint8_t *out_len)
{
    if (tg->apdu_len < 2u) {
        *out_len = 0u;
        return NULL;
    }
    if (tg->apdu_len == 2u) {
        /* Short encoding: data is in the low 6 bits of APDU[1] */
        s_short_data[0] = (uint8_t)(tg->apdu[1] & 0x3Fu);
        *out_len = 1u;
        return s_short_data;
    }
    /* Long encoding: data starts at APDU[2] */
    *out_len = (uint8_t)(tg->apdu_len - 2u);
    return &tg->apdu[2];
}

/* -------------------------------------------------------------------------
 * Serialization — KNX standard TP wire format
 * ---------------------------------------------------------------------- */

uint8_t knxTelegram_serialize(const KnxTelegram *tg, uint8_t *buf, uint8_t buf_len)
{
    /* Total frame length = 6-byte header + APDU + 1-byte checksum */
    uint8_t frame_len = (uint8_t)(6u + tg->apdu_len + 1u);
    if (buf_len < frame_len || tg->apdu_len < 2u) {
        return 0u;
    }

    buf[0] = tg->ctrl;
    buf[1] = (uint8_t)(tg->src >> 8);
    buf[2] = (uint8_t)(tg->src & 0xFFu);
    buf[3] = (uint8_t)(tg->dst >> 8);
    buf[4] = (uint8_t)(tg->dst & 0xFFu);
    /* DAF byte: addr-type | hop-count<<4 | (APDU_len-1) */
    buf[5] = (uint8_t)((tg->dst_is_group ? KNX_DAF_GROUP : KNX_DAF_INDIVIDUAL)
                     | ((tg->hop_count & 0x07u) << 4)
                     | ((tg->apdu_len - 1u) & 0x0Fu));
    memcpy(&buf[6], tg->apdu, tg->apdu_len);
    buf[frame_len - 1u] = compute_checksum(buf, (uint8_t)(frame_len - 1u));

    return frame_len;
}

bool knxTelegram_deserialize(KnxTelegram *tg, const uint8_t *buf, uint8_t len)
{
    if (len < KNX_FRAME_MIN_LEN) {
        return false;
    }

    /* APDU length N = (byte5 & 0x0F) + 1; total frame = N + 8 ... wait:
     * total = 6 (header) + N (apdu) + 1 (checksum) = N + 7.
     * But KNX_FRAME_MIN_LEN = 9 corresponds to N=2 minimum APDU → correct. */
    uint8_t apdu_len = (uint8_t)((buf[5] & 0x0Fu) + 1u);
    uint8_t frame_len = (uint8_t)(6u + apdu_len + 1u);

    if (len < frame_len) {
        return false;
    }

    /* Verify checksum */
    uint8_t expected_cs = compute_checksum(buf, (uint8_t)(frame_len - 1u));
    if (buf[frame_len - 1u] != expected_cs) {
        return false;
    }

    tg->ctrl         = buf[0];
    tg->src          = (KnxIndividualAddr)(((uint16_t)buf[1] << 8) | buf[2]);
    tg->dst          = (KnxGroupAddr)     (((uint16_t)buf[3] << 8) | buf[4]);
    tg->dst_is_group = (buf[5] & KNX_DAF_GROUP) != 0u;
    tg->hop_count    = (uint8_t)((buf[5] >> 4) & 0x07u);
    tg->apdu_len     = apdu_len;
    memcpy(tg->apdu, &buf[6], apdu_len);

    return true;
}

/* -------------------------------------------------------------------------
 * cEMI serialization — KNXnet/IP wire format
 * ---------------------------------------------------------------------- */

uint8_t knxTelegram_toCEMI(const KnxTelegram *tg,
                            uint8_t *buf, uint8_t buf_len,
                            uint8_t msg_code)
{
    /* cEMI header: msg_code(1) + addinfo_len(1) + ctrl1(1) + ctrl2(1) +
     *              src(2) + dst(2) + apdu_len_field(1) + apdu(N) */
    uint8_t cemi_len = (uint8_t)(8u + tg->apdu_len);
    if (buf_len < cemi_len || tg->apdu_len < 2u) {
        return 0u;
    }

    buf[0] = msg_code;
    buf[1] = 0x00u;  /* No additional info */
    buf[2] = tg->ctrl;
    /* cEMI ctrl2: addr-type | hop-count<<4 (no length field here — that is separate) */
    buf[3] = (uint8_t)((tg->dst_is_group ? KNX_DAF_GROUP : KNX_DAF_INDIVIDUAL)
                     | ((tg->hop_count & 0x07u) << 4));
    buf[4] = (uint8_t)(tg->src >> 8);
    buf[5] = (uint8_t)(tg->src & 0xFFu);
    buf[6] = (uint8_t)(tg->dst >> 8);
    buf[7] = (uint8_t)(tg->dst & 0xFFu);
    /* In cEMI, the APDU length field = APDU byte count - 1 */
    buf[8] = (uint8_t)(tg->apdu_len - 1u);
    memcpy(&buf[9], tg->apdu, tg->apdu_len);

    return cemi_len;
}

bool knxTelegram_fromCEMI(KnxTelegram *tg, const uint8_t *buf, uint8_t len)
{
    /* Minimum cEMI L_DATA frame: 8 bytes header + 2 bytes APDU */
    if (len < 10u) {
        return false;
    }

    /* Skip additional info block if present */
    uint8_t addinfo_len = buf[1];
    uint8_t hdr_end = (uint8_t)(2u + addinfo_len); /* offset of ctrl1 */

    if (len < (uint8_t)(hdr_end + 8u)) {
        return false;
    }

    uint8_t apdu_len = (uint8_t)(buf[hdr_end + 6u] + 1u); /* field = APDU_len - 1 */
    if (len < (uint8_t)(hdr_end + 7u + apdu_len)) {
        return false;
    }
    if (apdu_len > KNX_APDU_BUF_LEN) {
        return false;
    }

    tg->ctrl         = buf[hdr_end];
    /* cEMI ctrl2 is at hdr_end+1 */
    uint8_t ctrl2    = buf[hdr_end + 1u];
    tg->dst_is_group = (ctrl2 & KNX_DAF_GROUP) != 0u;
    tg->hop_count    = (uint8_t)((ctrl2 >> 4) & 0x07u);
    tg->src          = (KnxIndividualAddr)(((uint16_t)buf[hdr_end + 2u] << 8)
                                         |  (uint16_t)buf[hdr_end + 3u]);
    tg->dst          = (KnxGroupAddr)     (((uint16_t)buf[hdr_end + 4u] << 8)
                                         |  (uint16_t)buf[hdr_end + 5u]);
    tg->apdu_len     = apdu_len;
    memcpy(tg->apdu, &buf[hdr_end + 7u], apdu_len);

    return true;
}
