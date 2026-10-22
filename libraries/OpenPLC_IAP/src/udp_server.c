#include <string.h>
#include <stdio.h>
#include <stdlib.h>      /* strtoul */

#include "stm32_def.h"   /* HAL_GetTick */

#include "lwip/pbuf.h"
#include "lwip/udp.h"
#include "lwip/ip_addr.h"
#include "lwip/ip4_addr.h"
#include "lwip/netif.h"

#include "IAP_config.h"
#include "openplc_app_version.h"   /* the sketch defines openplc_app_version */
#include "IAP_boot_handoff.h"
#include "boot_log.h"
#include "iap_auth.h"
#include "iap_cert.h"
#include "iap_keyderive.h"
#include "owner_root_ro.h"
#include "fw_meta_ro.h"

#define REBOOT_COOLDOWN_MS 10000U
static uint32_t s_last_reboot_tick = 0u;

static struct udp_pcb *udp_server_pcb = NULL;
static volatile uint32_t udp_server_recv_counter = 0u;
static volatile uint32_t udp_server_last_rx_tick_ms = 0u;
static volatile uint16_t udp_server_last_rx_len_value = 0u;

/* Ceiling on how much traffic a stranger can make this device emit, so a
 * spoofed-source flood of discovery queries cannot turn it into a reflection
 * tool against a third party on the LAN.
 *
 * Device-wide, not per source: a per-source budget is shared by every program
 * on one host ($PROD/docs/tables/DECISIONS.md, decision 3). Normal load is
 * ~2 replies/s.
 *
 * Mirrored in the bootloader: OpenPLC_BootLoader/IAPServer/udp_server.c. Change
 * one, change both. */
#define DISCOVERY_MAX_REPLIES_PER_SEC 50U

static bool discovery_reply_allowed(void)
{
  static uint32_t window_start;
  static uint32_t replies_in_window;

  uint32_t now = HAL_GetTick();

  if ((now - window_start) >= 1000U) {
    window_start = now;
    replies_in_window = 0U;
  }

  if (replies_in_window >= DISCOVERY_MAX_REPLIES_PER_SEC) {
    /* Only the first refusal of each window speaks: the log port is far slower
     * than the flood, so per-packet logging would be the worse denial of service.
     * Why, in full: $PROD/docs/modules/M1/BOOT-SEQUENCE.md "网络发现：限流". */
    if (replies_in_window == DISCOVERY_MAX_REPLIES_PER_SEC) {
      replies_in_window++;
      printf("[UDP] discovery capped at %u replies/s - something is flooding us\r\n",
             (unsigned)DISCOVERY_MAX_REPLIES_PER_SEC);
    }
    return false;
  }

  replies_in_window++;
  return true;
}

static void openplc_udp_reply(struct udp_pcb *pcb, const ip_addr_t *addr, u16_t port, const char *msg)
{
  size_t len = strlen(msg);
  struct pbuf *reply_pbuf = pbuf_alloc(PBUF_TRANSPORT, (u16_t)len, PBUF_RAM);
  if (reply_pbuf == NULL) {
    return;
  }
  memcpy(reply_pbuf->payload, msg, len);
  udp_sendto(pcb, reply_pbuf, addr, port);
  pbuf_free(reply_pbuf);
}

/*
 * Ask the bootloader to stay in ethernet upload mode after the next reset.
 * boot_handoff_request() verifies the record landed before it resets.
 */
static void openplc_set_eth_flag_and_reset(void)
{
  /* Does not return on success. If it returns, the request was not stored, so
   * resetting would be worse than useless: the board would come straight back
   * into the app and the operator would see "no response" all over again. */
  if (!boot_handoff_request(BOOT_REQ_ETH)) {
    printf("Refusing to reset: ethernet boot request could not be stored\r\n");
  }
}

/* Lower-case hex only, as IAPTool sends it. Returns false on a length
 * mismatch or a non-hex character; out may then be partly written. */
static bool hex_decode_local(const char *hex, uint8_t *out, uint32_t out_len)
{
  uint32_t i;

  if (strlen(hex) != out_len * 2U) {
    return false;
  }
  for (i = 0; i < out_len; i++) {
    char hi = hex[i * 2U], lo = hex[i * 2U + 1U];
    int hi_v = (hi >= '0' && hi <= '9') ? hi - '0' : (hi >= 'a' && hi <= 'f') ? hi - 'a' + 10 : -1;
    int lo_v = (lo >= '0' && lo <= '9') ? lo - '0' : (lo >= 'a' && lo <= 'f') ? lo - 'a' + 10 : -1;
    if (hi_v < 0 || lo_v < 0) {
      return false;
    }
    out[i] = (uint8_t)((hi_v << 4) | lo_v);
  }
  return true;
}

/* "openplc_log [<from>]": the SRAM4 boot log, one reply-sized piece at a time.
 * Format and limits: $PROD/docs/modules/M1/IAP-PROTOCOL.md "openplc_log". */
#define LOG_READ_REPLY_MAX           1024U
#define LOG_READ_HEADER_MAX          32U     /* "log <u32> <0|1>\n" */
#define LOG_READ_MAX_REPLIES_PER_SEC 20U

/* Own budget so log reads cannot starve discovery; device-wide for the same
 * reason as discovery_reply_allowed(). */
static bool log_read_reply_allowed(void)
{
  static uint32_t log_win_start;
  static uint32_t log_replies;

  uint32_t now = HAL_GetTick();

  if ((now - log_win_start) >= 1000U) {
    log_win_start = now;
    log_replies = 0U;
  }
  if (log_replies >= LOG_READ_MAX_REPLIES_PER_SEC) {
    return false;
  }
  log_replies++;
  return true;
}

typedef struct {
  uint32_t from;
  uint32_t next;
  bool full;
  uint16_t used;
  char body[LOG_READ_REPLY_MAX - LOG_READ_HEADER_MAX];
} log_read_ctx_t;

/* boot_log_read_all() cannot stop early, so once full the rest is skipped. */
static void log_read_line(const char *line, size_t len, uint32_t seq, void *ctx)
{
  log_read_ctx_t *c = (log_read_ctx_t *)ctx;
  char seq_txt[12];
  int seq_len;
  size_t i;

  if ((seq < c->from) || c->full) {
    return;
  }
  seq_len = snprintf(seq_txt, sizeof(seq_txt), "%lu ", (unsigned long)seq);
  if ((seq_len <= 0) || ((c->used + (size_t)seq_len + len + 1U) > sizeof(c->body))) {
    c->full = true;
    return;
  }
  memcpy(&c->body[c->used], seq_txt, (size_t)seq_len);
  c->used += (uint16_t)seq_len;
  for (i = 0; i < len; i++) {
    /* One record, one line: the reply is line-framed. */
    c->body[c->used++] = ((line[i] == '\r') || (line[i] == '\n')) ? ' ' : line[i];
  }
  c->body[c->used++] = '\n';
  c->next = seq + 1U;
}

static void log_read_reply(struct udp_pcb *pcb, const ip_addr_t *addr, u16_t port, uint32_t from)
{
  /* Static: too big for the lwIP callback's stack; lwIP runs callbacks one at a time. */
  static log_read_ctx_t ctx;
  char header[LOG_READ_HEADER_MAX];
  int header_len;
  struct pbuf *reply_pbuf;

  if (!log_read_reply_allowed()) {
    return;
  }
  ctx.from = from;
  ctx.next = from;
  ctx.full = false;
  ctx.used = 0U;
  boot_log_read_all(log_read_line, &ctx);

  header_len = snprintf(header, sizeof(header), "log %lu %u\n",
                        (unsigned long)ctx.next, ctx.full ? 1U : 0U);
  if ((header_len <= 0) || ((size_t)header_len >= sizeof(header))) {
    return;
  }
  reply_pbuf = pbuf_alloc(PBUF_TRANSPORT, (u16_t)(header_len + ctx.used), PBUF_RAM);
  if (reply_pbuf == NULL) {
    return;
  }
  memcpy(reply_pbuf->payload, header, (size_t)header_len);
  memcpy((char *)reply_pbuf->payload + header_len, ctx.body, ctx.used);
  udp_sendto(pcb, reply_pbuf, addr, port);
  pbuf_free(reply_pbuf);
}

/* Read-only queries: getuid, getpubkey, getowner, getapprevoked.
 * Format and limits: $PROD/docs/modules/M1/IAP-PROTOCOL.md "只读查询".
 * Mirrored in the bootloader: OpenPLC_BootLoader/IAPServer/udp_server.c
 * (mirror 21). The four query_* accessors are this side's;
 * iap_query_reply() and query_reply_allowed() must stay textually identical
 * to the bootloader's. */
#define IAP_QUERY_REPLY_MAX       129U   /* getpubkey's 128 hex characters + NUL */
#define QUERY_MAX_REPLIES_PER_SEC 20U

/* Own budget, device-wide, for the same reason as discovery_reply_allowed(). */
static bool query_reply_allowed(void)
{
  static uint32_t query_win_start;
  static uint32_t query_replies;

  uint32_t now = HAL_GetTick();

  if ((now - query_win_start) >= 1000U) {
    query_win_start = now;
    query_replies = 0U;
  }
  if (query_replies >= QUERY_MAX_REPLIES_PER_SEC) {
    return false;
  }
  query_replies++;
  return true;
}

static bool query_root(uint8_t out[64])
{
  return owner_root_ro_get(out);
}

static uint32_t query_generation(void)
{
  return owner_root_ro_generation();
}

/* The leaf that signed the running image: the bootloader started it, so it
 * judged it valid, and only the latest metadata record is left to read. */
static bool query_installed_leaf(uint8_t out[64])
{
  return fw_meta_ro_installed_leaf(out);
}

static bool query_is_revoked(const uint8_t leaf[64])
{
  return owner_root_ro_is_revoked(leaf);
}

static bool iap_query_reply(const char *name, char *out, size_t out_len)
{
  uint8_t key[64];
  uint32_t i;

  if (out_len < IAP_QUERY_REPLY_MAX) {
    return false;
  }
  if (strcmp(name, "getuid") == 0) {
    iap_keyderive_get_machine_id_hex(out);
  } else if (strcmp(name, "getpubkey") == 0) {
    /* "none" without a root: IAPTool then claims the board (decision 72). */
    if (!query_root(key)) {
      (void)strcpy(out, "none");
    } else {
      for (i = 0U; i < 64U; i++) {
        (void)snprintf(&out[i * 2U], 3U, "%02x", key[i]);
      }
    }
  } else if (strcmp(name, "getowner") == 0) {
    (void)snprintf(out, out_len, "%lu", (unsigned long)query_generation());
  } else if (strcmp(name, "getapprevoked") == 0) {
    /* "none" is not "no": a board with nothing runnable has nothing to re-upload. */
    if (!query_installed_leaf(key)) {
      (void)strcpy(out, "none");
    } else {
      (void)strcpy(out, query_is_revoked(key) ? "yes" : "no");
    }
  } else {
    return false;
  }
  return true;
}

static void udp_server_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                            const ip_addr_t *addr, u16_t port)
{
  (void)arg;

  if ((p == NULL) || (p->payload == NULL) || (p->len == 0)) {
    if (p != NULL) {
      pbuf_free(p);
    }
    return;
  }

  /* "openplc_server_reboot <cert_hex> <noncesig_hex>" is ~408 bytes. */
  char recv_buf[512] = {0};
  char query_msg[IAP_QUERY_REPLY_MAX];
  const uint16_t n = (p->len < sizeof(recv_buf) - 1U) ? p->len : (sizeof(recv_buf) - 1U);
  memcpy(recv_buf, p->payload, n);
  udp_server_recv_counter++;
  udp_server_last_rx_tick_ms = HAL_GetTick();
  udp_server_last_rx_len_value = n;
  pbuf_free(p);

  if ((strcmp(recv_buf, "DISCOVER") == 0) ||
      (strcmp(recv_buf, "openplc_discover") == 0) ||
      (strcmp(recv_buf, "openplc_server_where_r_y") == 0) ||
      (strcmp(recv_buf, "ping") == 0)) {
    /* Identity string contract, shared with the bootloader's
     * iap_identity_string() (OpenPLC_BootLoader/IAPServer/IAP_server.c): the PC
     * tool splits it on "_", so it is exactly five fields --
     * name_uid_role_pkgversion_appversion -- and no field may contain an
     * underscore. The bootloader has nothing to report for the last one and
     * sends "-" there. The two repositories cannot share the code, so changing
     * one means changing both; the pair is tracked as a cross-repo mirror in
     * $PROD/docs/repo/ARCHITECTURE.md. */
    char uid_hex[IAP_MACHINE_ID_HEX_LEN + 1U] = {0};
    char reply_msg[96] = {0};

    if (!discovery_reply_allowed()) {
      return;
    }

    iap_keyderive_get_machine_id_hex(uid_hex);
    (void)snprintf(reply_msg, sizeof(reply_msg), "%s_%s_%s_%s_%s",
                   OPENPLC_DEVICE_NAME, uid_hex, UDP_SERVER_NAME, OPENPLC_FW_VERSION,
                   openplc_app_version);
    openplc_udp_reply(pcb, addr, port, reply_msg);
  } else if ((strncmp(recv_buf, "openplc_log", 11) == 0) &&
             ((recv_buf[11] == '\0') || (recv_buf[11] == ' '))) {
    log_read_reply(pcb, addr, port, (uint32_t)strtoul(&recv_buf[11], NULL, 10));
  } else if (iap_query_reply(recv_buf, query_msg, sizeof(query_msg))) {
    if (query_reply_allowed()) {
      openplc_udp_reply(pcb, addr, port, query_msg);
    }
  } else if (strcmp(recv_buf, "openplc_server_reboot_challenge") == 0) {
    char nonce_hex[IAP_AUTH_NONCE_SIZE * 2U + 1U];
    /* Silence rather than a predictable nonce: without a challenge the caller
     * cannot authorize a reboot at all, which is the safe end. */
    if (iap_auth_issue_challenge(nonce_hex)) {
      openplc_udp_reply(pcb, addr, port, nonce_hex);
    }
  } else if (strncmp(recv_buf, "openplc_server_reboot ", 22) == 0) {
    // "openplc_server_reboot <cert_hex> <noncesig_hex>" -- cert is the
    // IAP_CERT_SIZE-byte certificate (iap_cert.h) naming the leaf key that
    // signed sha256(nonce || "openplc_server_reboot"), nonce_sig is that
    // 64-byte ECDSA signature, both for the nonce most recently returned by
    // "openplc_server_reboot_challenge"
    char cert_hex[(IAP_CERT_SIZE * 2U) + 1U] = {0};
    char noncesig_hex[129] = {0};
    /* %256s is IAP_CERT_SIZE*2 written out: a scanf field width cannot be a
     * macro. Update it by hand with IAP_CERT_SIZE, here and in the
     * bootloader's IAP_server.c. */
    if (sscanf(recv_buf, "openplc_server_reboot %256s %128s", cert_hex, noncesig_hex) == 2) {
      iap_cert_t cert;
      uint8_t noncesig_bytes[64];
      bool decodedOk = hex_decode_local(cert_hex, (uint8_t *)&cert, IAP_CERT_SIZE)
                     && hex_decode_local(noncesig_hex, noncesig_bytes, 64U);

      if (decodedOk && iap_auth_verify_and_consume((const uint8_t *)"openplc_server_reboot", 21U, &cert, noncesig_bytes)) {
        /* Even a valid credential must not be able to hold the PLC in a reboot
         * loop; one accepted reboot per cooldown window is enough for any real
         * update flow. */
        uint32_t now = HAL_GetTick();
        if ((s_last_reboot_tick != 0U) && ((now - s_last_reboot_tick) < REBOOT_COOLDOWN_MS)) {
          printf("Reboot request ignored: still within cooldown\r\n");
        } else {
          s_last_reboot_tick = now;
          openplc_set_eth_flag_and_reset();
        }
      } else {
        printf("Rejected unauthenticated openplc_server_reboot request\r\n");
      }
    }
  }
}

void openplc_udp_server_start(void)
{
  if (udp_server_pcb != NULL) {
    return;
  }

  udp_server_pcb = udp_new();
  if (udp_server_pcb == NULL) {
    return;
  }

  if (udp_bind(udp_server_pcb, IP_ADDR_ANY, OPENPLC_SERVER_PORT) != ERR_OK) {
    udp_remove(udp_server_pcb);
    udp_server_pcb = NULL;
    return;
  }

  udp_recv(udp_server_pcb, udp_server_recv, NULL);
}

uint32_t openplc_udp_server_recv_count(void)
{
  return udp_server_recv_counter;
}

uint32_t openplc_udp_server_last_rx_tick(void)
{
  return udp_server_last_rx_tick_ms;
}

uint16_t openplc_udp_server_last_rx_len(void)
{
  return udp_server_last_rx_len_value;
}
