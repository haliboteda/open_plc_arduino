#include <string.h>
#include <stdio.h>

#include "stm32_def.h"   /* HAL_GetTick */

#include "lwip/pbuf.h"
#include "lwip/udp.h"
#include "lwip/ip_addr.h"
#include "lwip/ip4_addr.h"
#include "lwip/netif.h"

#include "IAP_config.h"
#include "IAP_boot_handoff.h"
#include "iap_auth.h"
#include "iap_keyderive.h"

extern struct netif gnetif;

#define REBOOT_COOLDOWN_MS 10000U
static uint32_t s_last_reboot_tick = 0u;

static void (*udp_reboot_callback)(void) = NULL;
static struct udp_pcb *udp_server_pcb = NULL;
static volatile uint32_t udp_server_start_counter = 0u;
static volatile uint32_t udp_server_recv_counter = 0u;
static volatile uint32_t udp_server_reply_counter = 0u;
static volatile uint32_t udp_server_bind_fail_counter = 0u;
static volatile uint32_t udp_server_last_rx_tick_ms = 0u;
static volatile uint16_t udp_server_last_rx_port_value = 0u;
static volatile uint16_t udp_server_last_rx_len_value = 0u;

/* Ceiling on how much traffic a stranger can make this device emit, so a
 * spoofed-source flood of discovery queries cannot turn it into a reflection
 * tool against a third party on the LAN.
 *
 * Deliberately device-wide rather than per source: a per-source budget is
 * shared by every program on one host, and the Arduino IDE's network_discovery
 * polls every 30s from the same host an operator flashes from -- so our own two
 * tools spent a day refusing each other. A legitimate load is ~2 replies/s,
 * twenty-five times under this cap.
 *
 * Mirrored in the bootloader: open_plc_cube_ide/IAPServer/udp_server.c. Change
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
    /* Only the first refusal of each window speaks: printing per dropped packet
     * would let a flood keep the UART busy, which is a better denial of service
     * than the flood it reports. */
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
  udp_server_reply_counter++;
  pbuf_free(reply_pbuf);
}

/*
 * Ask the bootloader to stay in ethernet upload mode after the next reset.
 *
 * This used to write an RTC backup register here, which meant remembering to
 * open the backup domain first -- and iap_auth's nonce counter closes it again
 * as the last thing it does, on every "openplc_server_reboot_challenge", i.e.
 * always immediately before this call. The write was therefore discarded, the
 * board reset, and the bootloader jumped straight back into the app with nothing
 * logged anywhere. boot_handoff_request() has no such hidden precondition and
 * verifies the record landed before it resets.
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

  char recv_buf[128] = {0};
  const uint16_t n = (p->len < sizeof(recv_buf) - 1U) ? p->len : (sizeof(recv_buf) - 1U);
  memcpy(recv_buf, p->payload, n);
  udp_server_recv_counter++;
  udp_server_last_rx_tick_ms = HAL_GetTick();
  udp_server_last_rx_port_value = port;
  udp_server_last_rx_len_value = n;
  pbuf_free(p);

  if ((strcmp(recv_buf, "DISCOVER") == 0) ||
      (strcmp(recv_buf, "openplc_discover") == 0) ||
      (strcmp(recv_buf, "openplc_server_where_r_y") == 0) ||
      (strcmp(recv_buf, "ping") == 0)) {
    /* Identity string contract, shared with the bootloader's
     * iap_identity_string() (open_plc_cube_ide/IAPServer/IAP_server.c): the PC
     * tool splits it on "_", so it is exactly four fields --
     * name_uid_role_version -- and no field may contain an underscore. The two
     * repositories cannot share the code, so changing one means changing both. */
    char uid_hex[IAP_MACHINE_ID_HEX_LEN + 1U] = {0};
    char reply_msg[96] = {0};

    if (!discovery_reply_allowed()) {
      return;
    }

    iap_keyderive_get_machine_id_hex(uid_hex);
    (void)snprintf(reply_msg, sizeof(reply_msg), "%s_%s_%s_%s",
                   OPENPLC_DEVICE_NAME, uid_hex, UDP_SERVER_NAME, OPENPLC_FW_VERSION);
    openplc_udp_reply(pcb, addr, port, reply_msg);
  } else if (strcmp(recv_buf, "openplc_server_reboot_challenge") == 0) {
    char nonce_hex[IAP_AUTH_NONCE_SIZE * 2U + 1U];
    iap_auth_issue_challenge(nonce_hex);
    openplc_udp_reply(pcb, addr, port, nonce_hex);
  } else if (strncmp(recv_buf, "openplc_server_reboot ", 22) == 0) {
    // "openplc_server_reboot <hmac_hex>" -- hmac must be
    // HMAC-SHA256(auth_key, nonce || "openplc_server_reboot") for the nonce
    // most recently returned by "openplc_server_reboot_challenge"
    char hmac_hex[65] = {0};
    if (sscanf(recv_buf, "openplc_server_reboot %64s", hmac_hex) == 1) {
      uint8_t hmac_bytes[IAP_AUTH_HMAC_SIZE];
      bool decodedOk = (strlen(hmac_hex) == IAP_AUTH_HMAC_SIZE * 2U);
      if (decodedOk) {
        uint32_t i;
        for (i = 0; i < IAP_AUTH_HMAC_SIZE && decodedOk; i++) {
          char hi = hmac_hex[i * 2U], lo = hmac_hex[i * 2U + 1U];
          int hi_v = (hi >= '0' && hi <= '9') ? hi - '0' : (hi >= 'a' && hi <= 'f') ? hi - 'a' + 10 : -1;
          int lo_v = (lo >= '0' && lo <= '9') ? lo - '0' : (lo >= 'a' && lo <= 'f') ? lo - 'a' + 10 : -1;
          if (hi_v < 0 || lo_v < 0) { decodedOk = false; break; }
          hmac_bytes[i] = (uint8_t)((hi_v << 4) | lo_v);
        }
      }

      if (decodedOk && iap_auth_verify_and_consume((const uint8_t *)"openplc_server_reboot", 21U, hmac_bytes)) {
        /* Even a valid credential must not be able to hold the PLC in a reboot
         * loop; one accepted reboot per cooldown window is enough for any real
         * update flow. */
        uint32_t now = HAL_GetTick();
        if ((s_last_reboot_tick != 0U) && ((now - s_last_reboot_tick) < REBOOT_COOLDOWN_MS)) {
          printf("Reboot request ignored: still within cooldown\r\n");
        } else {
          s_last_reboot_tick = now;
          if (udp_reboot_callback != NULL) {
            udp_reboot_callback();
          } else {
            openplc_set_eth_flag_and_reset();
          }
        }
      } else {
        printf("Rejected unauthenticated openplc_server_reboot request\r\n");
      }
    }
  }
}

void openplc_udp_server_start(void (*reboot_cb)(void))
{
  udp_reboot_callback = reboot_cb;
  udp_server_start_counter++;

  if (udp_server_pcb != NULL) {
    return;
  }

  udp_server_pcb = udp_new();
  if (udp_server_pcb == NULL) {
    return;
  }

  if (udp_bind(udp_server_pcb, IP_ADDR_ANY, OPENPLC_SERVER_PORT) != ERR_OK) {
    udp_server_bind_fail_counter++;
    udp_remove(udp_server_pcb);
    udp_server_pcb = NULL;
    return;
  }

  udp_recv(udp_server_pcb, udp_server_recv, NULL);
}

void openplc_udp_server_stop(void)
{
  if (udp_server_pcb != NULL) {
    udp_recv(udp_server_pcb, NULL, NULL);
    udp_disconnect(udp_server_pcb);
    udp_remove(udp_server_pcb);
    udp_server_pcb = NULL;
  }
}

uint32_t openplc_udp_server_start_count(void)
{
  return udp_server_start_counter;
}

uint32_t openplc_udp_server_recv_count(void)
{
  return udp_server_recv_counter;
}

uint32_t openplc_udp_server_reply_count(void)
{
  return udp_server_reply_counter;
}

uint32_t openplc_udp_server_bind_fail_count(void)
{
  return udp_server_bind_fail_counter;
}

uint32_t openplc_udp_server_last_rx_tick(void)
{
  return udp_server_last_rx_tick_ms;
}

uint16_t openplc_udp_server_last_rx_port(void)
{
  return udp_server_last_rx_port_value;
}

uint16_t openplc_udp_server_last_rx_len(void)
{
  return udp_server_last_rx_len_value;
}
