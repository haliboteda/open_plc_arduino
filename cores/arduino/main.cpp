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
}
HardwareSerial Serial_Test(PC_11, PC_10);
bool g_ip_uart_done = false;
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

#ifdef OPENPLC_UDP_SERVER_AUTOSTART
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

    if (!g_ip_uart_done && openplc_net_has_ip()) {
      uint8_t ip[4] = {0};
      if (openplc_net_get_ipv4(ip)) {
        Serial_Test.begin(115200);
        Serial_Test.print("IP: ");
        Serial_Test.print(ip[0]); Serial_Test.print(".");
        Serial_Test.print(ip[1]); Serial_Test.print(".");
        Serial_Test.print(ip[2]); Serial_Test.print(".");
        Serial_Test.println(ip[3]);
        Serial_Test.flush();
        Serial_Test.end();      // release UART
        g_ip_uart_done = true;
      }
    }
#endif

    loop();
    serialEventRun();
  }

  return 0;
}
