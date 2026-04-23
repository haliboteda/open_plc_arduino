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
bool g_ip_uart_done = false;
bool g_diag_uart_ready = false;
uint32_t g_last_diag_ms = 0;
uint32_t g_boot_ms = 0;

static const char *openplc_reset_cause()
{
  if (__HAL_RCC_GET_FLAG(RCC_FLAG_WWDG1RST)) return "WWDG";
  if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDG1RST)) return "IWDG";
  if (__HAL_RCC_GET_FLAG(RCC_FLAG_SFTRST)) return "SOFT";
  if (__HAL_RCC_GET_FLAG(RCC_FLAG_PORRST)) return "POR";
  if (__HAL_RCC_GET_FLAG(RCC_FLAG_PINRST)) return "PIN";
  if (__HAL_RCC_GET_FLAG(RCC_FLAG_BORRST)) return "BOR";
#if defined(RCC_FLAG_D2RST)
  if (__HAL_RCC_GET_FLAG(RCC_FLAG_D2RST)) return "D2RST";
#endif
  return "UNKNOWN";
}

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
  Serial_Test.print("[BOOT] cause=");
  Serial_Test.print(openplc_reset_cause());
  Serial_Test.print(" millis=");
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
  Serial_Test.println(ip[3]);
}

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
  pinMode(PB_10, OUTPUT);
#endif

  setup();

  for (;;) {
#if defined(CORE_CALLBACK)
    CoreCallback();
#endif

#ifdef OPENPLC_UDP_SERVER_AUTOSTART
    openplc_net_process();

    // if (!g_ip_uart_done && openplc_net_has_ip()) {
    //   openplc_diag_print_ip();
    //   g_ip_uart_done = true;
    // }

    // if ((millis() - g_last_diag_ms) >= 5000UL) {
    //   g_last_diag_ms = millis();
    //   openplc_diag_heartbeat();
    // }
#endif

    loop();
    serialEventRun();
  }

  return 0;
}
