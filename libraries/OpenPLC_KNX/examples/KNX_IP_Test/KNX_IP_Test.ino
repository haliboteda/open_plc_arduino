/*
 * KNX_IP_Test - Phase 1 IP transport test for OpenPLC_KNX.
 *
 * PURPOSE
 *   Verify the KNXnet/IP stack end-to-end:
 *     - Ethernet + LwIP brings up DHCP address
 *     - ETS can discover this device on the LAN
 *     - ETS can assign an individual address and download an application
 *     - Group writes from ETS group monitor drive the relay outputs
 *
 * REQUIRED BUILD SETTINGS (Arduino IDE)
 *   Board : OPEN-PLC → PLC H743
 *   KNX Role : IP+TP Device (both transports, MASK 0x5780)  [DEFAULT - no change needed]
 *   USB  : CDC (generic 'Serial' supersede U(S)ART)          ← for Serial output
 *
 * HARDWARE NEEDED
 *   - OpenPLC Bridge board with STM32H743
 *   - Ethernet cable to LAN (DHCP router present)
 *   - PC with ETS5/ETS6 on the same LAN
 *   - No KNX TP bus needed for this test
 *
 * ETS PROJECT SETUP
 *   Create a new ETS project.  Add a "Generic" device, set mask version 0x5780.
 *   In the Group Object Table, create two GOs:
 *     GO #1  DPT-1.001  Send/Receive  - link to group address 0/0/1
 *     GO #2  DPT-1.001  Send/Receive  - link to group address 0/0/2
 *   Download to device after individual address programming.
 *
 * EXPECTED RESULTS (in order)
 *   Serial output:
 *     [NET] Ethernet init...
 *     [NET] Waiting for DHCP...
 *     [NET] IP address: 192.168.x.x
 *     [KNX] Stack started (MASK 0x57B0)
 *     [KNX] Not yet configured by ETS - waiting...
 *     ...after ETS programs the device...
 *     [KNX] Configured! Individual address: 1.1.1
 *   ETS:
 *     Device visible in "Network Interfaces" or direct IP search
 *     Individual address can be assigned (prog LED on PG11 lights during prog mode)
 *     Application downloads without error
 *   After download:
 *     Send switch ON to 0/0/1 → relay on PE6 closes, Serial prints "Relay 0 ON"
 *     Send switch OFF to 0/0/1 → relay on PE6 opens, Serial prints "Relay 0 OFF"
 *     Same for 0/0/2 → relay on PE5
 */

#include <OpenPLC_KNX.h>
#include <OpenPLC_Net_Autostart.h>
#include <OpenPLC_IAP_Autostart.h>

/* Relay group-object callbacks - called when ETS / another KNX device
 * sends a GroupValue.Write to the group address linked to GO #1 or #2. */

static void onRelay0(GroupObject &go)
{
    bool on = (bool)go.value(DPT_Switch);
    KNXHelper.setRelayChannel(0, on);
    Serial_Test.print("[KNX] Relay 0 ");
    Serial_Test.println(on ? "ON" : "OFF");
}

static void onRelay1(GroupObject &go)
{
    bool on = (bool)go.value(DPT_Switch);
    KNXHelper.setRelayChannel(1, on);
    Serial_Test.print("[KNX] Relay 1 ");
    Serial_Test.println(on ? "ON" : "OFF");
}

/* -------------------------------------------------------------------------
 * State tracking for one-time console messages
 * ---------------------------------------------------------------------- */
static bool s_netReported  = false;
static bool s_knxConfigured = false;

static void printIp(void)
{
    unsigned char ip[4];
    if (openplc_net_get_ipv4(ip)) {
        Serial_Test.print("[NET] IP address: ");
        Serial_Test.print(ip[0]); Serial_Test.print(".");
        Serial_Test.print(ip[1]); Serial_Test.print(".");
        Serial_Test.print(ip[2]); Serial_Test.print(".");
        Serial_Test.println(ip[3]);
    }
}

static void printIndividualAddr(uint16_t addr)
{
    Serial_Test.print("[KNX] Individual address: ");
    Serial_Test.print((addr >> 12) & 0xF);
    Serial_Test.print(".");
    Serial_Test.print((addr >> 8) & 0xF);
    Serial_Test.print(".");
    Serial_Test.println(addr & 0xFF);
}

static void printLwipDiag(void)
{
    Serial_Test.print("[LWIP] stats=");
    Serial_Test.print(openplc_lwip_stats_enabled() ? "on" : "off");
    Serial_Test.print(" mem_avail=");
    Serial_Test.print(openplc_lwip_mem_avail());
    Serial_Test.print(" mem_used=");
    Serial_Test.print(openplc_lwip_mem_used());
    Serial_Test.print(" mem_max=");
    Serial_Test.print(openplc_lwip_mem_max());
    Serial_Test.print(" udp_used=");
    Serial_Test.print(openplc_lwip_udp_pcb_used());
    Serial_Test.print(" udp_max=");
    Serial_Test.print(openplc_lwip_udp_pcb_max());
    Serial_Test.print(" udp_err=");
    Serial_Test.print(openplc_lwip_udp_pcb_err());
    Serial_Test.print(" pbuf_used=");
    Serial_Test.print(openplc_lwip_pbuf_used());
    Serial_Test.print(" pbuf_max=");
    Serial_Test.print(openplc_lwip_pbuf_max());
    Serial_Test.print(" pbuf_err=");
    Serial_Test.print(openplc_lwip_pbuf_err());
    Serial_Test.print(" igmp_used=");
    Serial_Test.print(openplc_lwip_igmp_group_used());
    Serial_Test.print(" igmp_max=");
    Serial_Test.print(openplc_lwip_igmp_group_max());
    Serial_Test.print(" igmp_err=");
    Serial_Test.println(openplc_lwip_igmp_group_err());
}

/* -------------------------------------------------------------------------
 * setup()
 * ---------------------------------------------------------------------- */
void setup()
{
    Serial_Test.begin(115200);
    delay(400);
    Serial_Test.println("[NET] Ethernet init...");

    /* 1. Start Ethernet + LwIP (DHCP). Must be called before KNX setup
     *    because the KNX platform reads netif_default for IP/MAC info. */
    openplc_net_init();

    /* 2. Wait up to 15 s for a DHCP lease. */
    Serial_Test.println("[NET] Waiting for DHCP...");
    uint32_t t0 = millis();
    while (!openplc_net_has_ip()) {
        openplc_net_process();
        if ((millis() - t0) > 15000u) {
            Serial_Test.println("[NET] DHCP timeout - using 0.0.0.0 (check cable/router)");
            break;
        }
    }
    printIp();
    printLwipDiag();

    /* Keep the OpenPLC discovery UDP server running by default so this
     * sketch does not change the board's normal discovery behaviour.
     * If you specifically want it disabled during KNXnet/IP testing,
     * compile with -DOPENPLC_KNX_TEST_STOP_DISCOVERY_UDP=1. */
#if defined(OPENPLC_KNX_TEST_STOP_DISCOVERY_UDP) && (OPENPLC_KNX_TEST_STOP_DISCOVERY_UDP)
    openplc_udp_server_stop();
    Serial_Test.println("[NET] OpenPLC UDP server stopped for KNX IP test");
#endif

    /* 3. Initialise KNX stack.
     *    KNXHelper.setup() loads the KNX ETS NVM (individual address,
     *    address tables) and configures the prog-button/LED GPIOs.
     *    The role (MASK_VERSION 0x57B0 = IP device) is set by the board
     *    variant selected in Arduino IDE. */
    KNXHelper.setup("OPENPLC000001");

    /* 4. Initialise 2-channel relay profile (PI8 and PI10). */
    KNXHelper.initRelayProfile2CH();

    /* 5. Self-program KNX tables if not already done by ETS.
     *    This must be called BEFORE KNX.configured() is first called so the
     *    _configured flag is not latched to false.
     *    No-op if the device was already programmed (tables LS_LOADED). */
    if (KNXHelper.selfProgram2CH(0x1101u)) {
        Serial_Test.println("[KNX] selfProgram2CH: tables ready (firmware or ETS).");
    } else {
        Serial_Test.println("[KNX] selfProgram2CH: FAILED - relay callbacks not registered.");
    }

    /* 6. Register group-object callbacks - only safe after tables are loaded. */
    if (KNX.configured()) {
        KNX.getGroupObject(1).callback(onRelay0);
        KNX.getGroupObject(2).callback(onRelay1);
    }

    /* 7. Start the KNX transport. After this call the device responds to
     *    KNXnet/IP search requests on 224.0.23.12:3671. */
    KNXHelper.start();

    Serial_Test.println("[KNX] Stack started (MASK 0x5780 - IP+TP dual device)");
    if (KNX.configured()) {
        Serial_Test.println("[KNX] Already configured by ETS:");
        printIndividualAddr(KNX.individualAddress());
    } else {
        Serial_Test.println("[KNX] Not yet configured - use ETS to assign address.");
        Serial_Test.println("[KNX] Press button on PG9 to enter programming mode.");
    }
}

/* -------------------------------------------------------------------------
 * loop()
 * ---------------------------------------------------------------------- */
static uint32_t s_statusMs = 0u;

void loop()
{
    /* Ethernet packet pump - must be called at least every few ms. */
    openplc_net_process();

    /* KNX stack: receive frames, run protocol timers, handle prog-mode. */
    KNXHelper.loop();

    /* One-time report when ETS configures the device mid-run. */
    if (!s_knxConfigured && KNX.configured()) {
        s_knxConfigured = true;
        Serial_Test.println("[KNX] Configured by ETS:");
        printIndividualAddr(KNX.individualAddress());
        /* Re-register callbacks (group object table may have changed). */
        KNX.getGroupObject(1).callback(onRelay0);
        KNX.getGroupObject(2).callback(onRelay1);
    }

    /* Periodic status every 10 s. */
    if ((millis() - s_statusMs) >= 10000u) {
        s_statusMs = millis();
        Serial_Test.print("[STATUS] IP OK=");
        Serial_Test.print(openplc_net_has_ip() ? "Y" : "N");
        Serial_Test.print("  KNX configured=");
        Serial_Test.print(KNX.configured() ? "Y" : "N");
        Serial_Test.print("  Prog mode=");
        Serial_Test.print(KNXHelper.progMode() ? "ON" : "off");
        Serial_Test.print("  TP VCC=");
        Serial_Test.print(KNXHelper.tpVccOk() ? "ok" : "--");
        Serial_Test.println();
        // printLwipDiag();
        // Serial_Test.print("[UDP] start=");
        // Serial_Test.print(openplc_udp_server_start_count());
        // Serial_Test.print(" rx=");
        // Serial_Test.print(openplc_udp_server_recv_count());
        // Serial_Test.print(" tx=");
        // Serial_Test.print(openplc_udp_server_reply_count());
        // Serial_Test.print(" bind_fail=");
        // Serial_Test.println(openplc_udp_server_bind_fail_count());
    }
}
