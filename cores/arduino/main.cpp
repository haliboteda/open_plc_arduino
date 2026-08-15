/*
  main.cpp - Main loop for Arduino sketches
  Copyright (c) 2005-2013 Arduino Team.  All right reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#define ARDUINO_MAIN
#include "Arduino.h"
#include "rtc.h"

#ifdef OPENPLC_UDP_SERVER_AUTOSTART
extern "C" {
  void openplc_net_init(void);
  void openplc_net_process(void);
  void openplc_udp_server_start(void (*reboot_cb)(void));
  uint8_t openplc_net_get_ipv4(uint8_t out[4]);
  uint8_t openplc_net_get_mac(uint8_t out[6]);
  uint8_t openplc_net_has_ip(void);
  uint32_t openplc_udp_server_start_count(void);
  uint32_t openplc_udp_server_recv_count(void);
  uint32_t openplc_udp_server_reply_count(void);
  uint32_t openplc_udp_server_bind_fail_count(void);
  uint32_t openplc_udp_server_last_rx_tick(void);
  uint16_t openplc_udp_server_last_rx_port(void);
  uint16_t openplc_udp_server_last_rx_len(void);
}
HardwareSerial Serial_Test(PC_11, PC_10);
#define OPENPLC_DIAG_PERIOD_MS 5000U
bool g_ip_uart_done = false;
bool g_diag_uart_ready = false;
uint32_t g_last_diag_ms = 0;
uint32_t g_boot_ms = 0;

static void openplc_diag_begin_uart()
{
  if (!g_diag_uart_ready) {
    Serial_Test.begin(115200);
    g_diag_uart_ready = true;
  }
}

static void openplc_diag_boot_banner()
{
  openplc_diag_begin_uart();
  /* No reset cause here: the bootloader clears RCC->RSR before jumping to us,
   * so it is the only image that can report it -- see its "** Reset cause:" line. */
  Serial_Test.print("[BOOT] millis=");
  Serial_Test.println(millis());
}

static void openplc_diag_print_ip()
{
  uint8_t ip[4] = {0};
  if (!openplc_net_get_ipv4(ip)) {
    return;
  }

  openplc_diag_begin_uart();
  Serial_Test.print("[NET] ip=");
  Serial_Test.print(ip[0]); Serial_Test.print(".");
  Serial_Test.print(ip[1]); Serial_Test.print(".");
  Serial_Test.print(ip[2]); Serial_Test.print(".");
  Serial_Test.print(ip[3]);

  uint8_t mac[6] = {0};
  if (openplc_net_get_mac(mac)) {
    Serial_Test.print(" mac=");
    for (uint8_t i = 0; i < 6; i++) {
      if (i) Serial_Test.print(":");
      if (mac[i] < 0x10) Serial_Test.print("0");
      Serial_Test.print(mac[i], HEX);
    }
  }
  Serial_Test.println();
}

/* Off by default: build with -DOPENPLC_DIAG_HEARTBEAT to trace the UDP server. */
#ifdef OPENPLC_DIAG_HEARTBEAT
static void openplc_diag_heartbeat()
{
  openplc_diag_begin_uart();
  Serial_Test.print("[HB] up_ms=");
  Serial_Test.print(millis() - g_boot_ms);
  Serial_Test.print(" has_ip=");
  Serial_Test.print(openplc_net_has_ip());
  Serial_Test.print(" udp_start=");
  Serial_Test.print(openplc_udp_server_start_count());
  Serial_Test.print(" udp_rx=");
  Serial_Test.print(openplc_udp_server_recv_count());
  Serial_Test.print(" udp_tx=");
  Serial_Test.print(openplc_udp_server_reply_count());
  Serial_Test.print(" bind_fail=");
  Serial_Test.print(openplc_udp_server_bind_fail_count());
  Serial_Test.print(" last_rx_ms=");
  Serial_Test.print(openplc_udp_server_last_rx_tick());
  Serial_Test.print(" last_rx_port=");
  Serial_Test.print(openplc_udp_server_last_rx_port());
  Serial_Test.print(" last_rx_len=");
  Serial_Test.println(openplc_udp_server_last_rx_len());
}
#endif

/* Report address and MAC once they arrive. */
static void openplc_diag_tick()
{
  if (!g_ip_uart_done && openplc_net_has_ip()) {
    openplc_diag_print_ip();
    g_ip_uart_done = true;
  }

#ifdef OPENPLC_DIAG_HEARTBEAT
  if ((millis() - g_last_diag_ms) >= OPENPLC_DIAG_PERIOD_MS) {
    g_last_diag_ms = millis();
    openplc_diag_heartbeat();
  }
#endif
}
#endif



// Force init to be called *first*, i.e. before static object allocation.
// Otherwise, statically allocated objects that need HAL may fail.
__attribute__((constructor(101))) void premain()
{

  // Required by FreeRTOS, see http://www.freertos.org/RTOS-Cortex-M3-M4.html
#ifdef NVIC_PRIORITYGROUP_4
  HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);
#endif
#if (__CORTEX_M == 0x07U)
  // Defined in CMSIS core_cm7.h
#ifndef I_CACHE_DISABLED
  SCB_EnableICache();
#endif
#ifndef D_CACHE_DISABLED
  SCB_EnableDCache();
#endif
#endif

  init();
  
  MX_RTC_Init();
}

/*
 * \brief Main entry point of Arduino application
 */
int main(void)
{
  initVariant();
  g_boot_ms = millis();

#ifdef OPENPLC_UDP_SERVER_AUTOSTART
  openplc_diag_boot_banner();
  openplc_net_init();
  openplc_udp_server_start(NULL);
  pinMode(RS232_EN_Pin, OUTPUT); 
#endif

  setup();

  for (;;) {
#if defined(CORE_CALLBACK)
    CoreCallback();
#endif

#ifdef OPENPLC_UDP_SERVER_AUTOSTART
    openplc_net_process();
    openplc_diag_tick();
#endif

    loop();
    serialEventRun();
  }

  return 0;
}
