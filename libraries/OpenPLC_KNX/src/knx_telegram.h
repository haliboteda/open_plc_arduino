#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "knx_address.h"
#include "knx_config.h"

/*
 * KNX standard frame (TP layer) and cEMI frame (KNXnet/IP layer).
 *
 * Standard frame wire layout (KNX spec Part 3.2.2):
 *
 *  Byte 0   : Control field 1 (CTRL1)
 *  Bytes 1-2: Source individual address (big-endian)
 *  Bytes 3-4: Destination address (big-endian)
 *  Byte  5  : DAF|HOP|LEN  (bit7=addr type, bits6-4=hop count, bits3-0=APDU_len-1)
 *  Bytes 6+ : APDU  (N bytes, where N = (byte5 & 0x0F) + 1)
 *  Last byte: Checksum  = 0xFF XOR (XOR of all preceding bytes)
 *
 * Total frame length = N + 8  (6-byte header + N-byte APDU + 1-byte checksum)
 *
 * APDU format for group communication (T_DATA_GROUP, TPCI=0b00):
 *  APDU[0] : TPCI (bits7-2) + APCI[9:8] (bits1-0)  -- always 0x00 for group data
 *  APDU[1] : APCI[7:0]  (GroupValueRead=0x00, Response=0x40, Write=0x80)
 *            For short data (DPT-1/2/3, ≤6 bits): data packed into APDU[1] bits[5:0]
 *  APDU[2+]: Long data bytes (DPT-5 and above)
 *
 * cEMI frame (KNXnet/IP, KNX spec 3.6.3):
 *  Byte 0  : Message code (0x29 = L_DATA.ind, 0x11 = L_DATA.req)
 *  Byte 1  : Additional info length (0x00)
 *  Bytes 2-: Same field layout as standard frame bytes 0-4 (ctrl, src, dst)
 *  Byte  6 : DAF|HOP|LEN (same as standard frame byte 5)
 *  Bytes 7+: APDU
 */

/* Control field 1 values */
#define KNX_CTRL_STD_NO_REPEAT  0xBCu   /* Standard frame, not repeated, priority Low */
#define KNX_CTRL_STD_REPEATED   0xB8u   /* Standard frame, repeated, priority Low */
#define KNX_CTRL_PRIO_SYSTEM    0x00u   /* Priority bits mask: System */
#define KNX_CTRL_PRIO_ALARM     0x08u   /* Priority bits mask: Alarm/Urgent */
#define KNX_CTRL_PRIO_HIGH      0x04u   /* Priority bits mask: High */
#define KNX_CTRL_PRIO_LOW       0x0Cu   /* Priority bits mask: Low (default) */

/* DAF byte helpers */
#define KNX_DAF_GROUP           0x80u   /* Destination is a group address */
#define KNX_DAF_INDIVIDUAL      0x00u   /* Destination is an individual address */
#define KNX_DAF_HOP6            0x60u   /* Hop count = 6 (default) */
/* Macro: DAF byte for group address, hop 6, APDU length N bytes */
#define KNX_DAF(n)              (uint8_t)(KNX_DAF_GROUP | KNX_DAF_HOP6 | (((n) - 1u) & 0x0Fu))

/* APCI service codes (lower 8 bits of the 10-bit APCI, APDU[1]) */
#define KNX_APCI_GROUP_READ     0x00u   /* GroupValue.Read */
#define KNX_APCI_GROUP_RESP     0x40u   /* GroupValue.Response */
#define KNX_APCI_GROUP_WRITE    0x80u   /* GroupValue.Write */
#define KNX_APCI_SERVICE_MASK   0xC0u   /* Mask for service code in APDU[1] */

/* cEMI message codes */
#define KNX_CEMI_L_DATA_REQ     0x11u   /* L_DATA.req: MCU sending to bus */
#define KNX_CEMI_L_DATA_CON     0x2Eu   /* L_DATA.con: confirmation from bus */
#define KNX_CEMI_L_DATA_IND     0x29u   /* L_DATA.ind: frame received from bus */

/* Minimum / maximum standard frame sizes */
#define KNX_FRAME_MIN_LEN       9u      /* ctrl+src(2)+dst(2)+daf+apdu(2)+checksum */
#define KNX_FRAME_MAX_LEN       (6u + KNX_APDU_BUF_LEN + 1u)

typedef enum {
    KNX_SVC_GROUP_READ  = 0,
    KNX_SVC_GROUP_RESP  = 1,
    KNX_SVC_GROUP_WRITE = 2,
    KNX_SVC_UNKNOWN     = 0xFF
} KnxService;

/*
 * In-memory representation of a decoded KNX standard frame.
 * Not the on-wire format — call knxTelegram_serialize / knxTelegram_deserialize
 * to convert.
 */
typedef struct {
    uint8_t           ctrl;                     /* Control field 1 */
    KnxIndividualAddr src;                      /* Source individual address */
    KnxGroupAddr      dst;                      /* Destination address */
    bool              dst_is_group;             /* true = group address, false = individual */
    uint8_t           hop_count;                /* Remaining hop count (0-7) */
    uint8_t           apdu[KNX_APDU_BUF_LEN];  /* Full APDU bytes (TPCI+APCI+data) */
    uint8_t           apdu_len;                 /* Number of valid bytes in apdu[] */
} KnxTelegram;

#ifdef __cplusplus
extern "C" {
#endif

/* --- Telegram building --- */

/* Initialize tg as a GroupValue.Write telegram.
 * data[] holds the raw DPT payload (1 byte for DPT-1/5, 2 for DPT-9, 4 for DPT-14).
 * Short data (1 byte, value ≤ 0x3F) is packed directly into APCI. */
void knxTelegram_buildGroupWrite(KnxTelegram *tg,
                                 KnxIndividualAddr src,
                                 KnxGroupAddr dst,
                                 const uint8_t *data, uint8_t data_len);

/* Initialize tg as a GroupValue.Read telegram. */
void knxTelegram_buildGroupRead(KnxTelegram *tg,
                                KnxIndividualAddr src,
                                KnxGroupAddr dst);

/* Initialize tg as a GroupValue.Response telegram. */
void knxTelegram_buildGroupResp(KnxTelegram *tg,
                                KnxIndividualAddr src,
                                KnxGroupAddr dst,
                                const uint8_t *data, uint8_t data_len);

/* --- Telegram inspection --- */

/* Return the service type encoded in the telegram's APDU. */
KnxService knxTelegram_service(const KnxTelegram *tg);

/* Return a pointer to the data payload bytes within the APDU, and the length.
 * For short (1-byte) data, the single byte is reconstructed into out_byte[0]
 * and *out_len is set to 1.  Returns NULL if the frame has no data. */
const uint8_t *knxTelegram_data(const KnxTelegram *tg, uint8_t *out_len);

/* --- Serialization (standard TP wire format) --- */

/* Serialize tg to a KNX standard TP frame in buf[].
 * Returns the number of bytes written, or 0 on error (buf too small). */
uint8_t knxTelegram_serialize(const KnxTelegram *tg, uint8_t *buf, uint8_t buf_len);

/* Deserialize a KNX standard TP frame from buf[] into tg.
 * Returns true if the frame is valid (correct length and checksum). */
bool knxTelegram_deserialize(KnxTelegram *tg, const uint8_t *buf, uint8_t len);

/* --- cEMI serialization (KNXnet/IP wire format) --- */

/* Serialize tg as a cEMI L_DATA frame in buf[].
 * msg_code should be KNX_CEMI_L_DATA_REQ for frames sent by this device.
 * Returns number of bytes written, or 0 on error. */
uint8_t knxTelegram_toCEMI(const KnxTelegram *tg,
                            uint8_t *buf, uint8_t buf_len,
                            uint8_t msg_code);

/* Deserialize a cEMI L_DATA frame from buf[] into tg.
 * Returns true if the frame is valid. */
bool knxTelegram_fromCEMI(KnxTelegram *tg, const uint8_t *buf, uint8_t len);

#ifdef __cplusplus
}
#endif
