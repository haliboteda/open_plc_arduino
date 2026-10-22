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
#include "openplc_net.h"

/* The RS232 terminals C05/C06, for the sketch only: the core never opens it
 * (R3-23); RS232.begin() powers the transceiver.
 *
 * ALT1 is load-bearing: it selects USART3 (AF7) on these pins instead of UART4
 * (AF8). Plain PC_11/PC_10 resolve to UART4 -- the same peripheral Serial4 uses
 * on PH13/PH14 -- and uart_handlers[] holds one handler per peripheral, so
 * whichever begin() ran last took the port and the other object went dead.
 *
 * The wires do not change: both peripherals reach the same two pins. */
RS232Serial RS232(PC_11_ALT1, PC_10_ALT1, RS232_EN);
/* The RS485 terminals; the object flips RS485_DIR itself ($PROD/docs/modules/M3/USER-API.md). */
RS485Serial RS485(RS485_RX, RS485_TX, RS485_DIR);
OpenPLCNet Net;
static bool s_ip_logged;

static void openplc_log_boot()
{
  char line[24];
  int n = snprintf(line, sizeof(line), "millis=%lu", (unsigned long)millis());

  /* No reset cause here: the bootloader logs it, and a sketch reads it with
   * openplcResetCause() (decision 80). */
  openplc_syslog_write(BOOT_LOG_TAG_BOOT, line, (size_t)n);
}

static void openplc_log_ip()
{
  uint8_t ip[4] = {0};
  uint8_t mac[6] = {0};
  char line[64];
  int n;

  if (!openplc_net_get_ipv4(ip)) {
    return;
  }
  n = snprintf(line, sizeof(line), "ip=%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
  if (openplc_net_get_mac(mac)) {
    n += snprintf(line + n, sizeof(line) - (size_t)n, " mac=%02X:%02X:%02X:%02X:%02X:%02X",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  }
  openplc_syslog_write(BOOT_LOG_TAG_NET, line, (size_t)n);
}

/* Log address and MAC once they arrive. */
static void openplc_diag_tick()
{
  if (!s_ip_logged && openplc_net_has_ip()) {
    openplc_log_ip();
    s_ip_logged = true;
  }
}
#endif



// Force init to be called *first*, i.e. before static object allocation.
// Otherwise, statically allocated objects that need HAL may fail.
__attribute__((constructor(101))) void premain()
{
  /* First, so static constructors can log. false: the bootloader has already
   * written the area since power-on ($PROD/docs/modules/M1/BOOT-LOG.md). */
  boot_log_init(false);

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

#ifdef OPENPLC_UDP_SERVER_AUTOSTART
static bool s_net_started;
#endif

/* delay() spins on yield(); service the network and the system log there too,
 * or the board is deaf for as long as the sketch waits. See OpenPLC_Instruction
 * docs/modules/M3-app-runtime.md, R3-07. */
extern "C" void yield(void)
{
  static bool busy;
  if (busy) {
    return;
  }
  busy = true;
#ifdef OPENPLC_UDP_SERVER_AUTOSTART
  if (s_net_started) {
    openplc_net_process();
  }
#endif
  openplc_syslog_service();
  busy = false;
}

/*
 * \brief Main entry point of Arduino application
 */
int main(void)
{
  initVariant();

#ifdef OPENPLC_UDP_SERVER_AUTOSTART
  openplc_log_boot();
  openplc_net_init();
  openplc_udp_server_start();
  s_net_started = true;
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
    openplc_syslog_service();
  }

  return 0;
}
