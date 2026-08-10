/*
 * KNX_TP_Sender - Continuous KNX TP transmit test.
 *
 * Sends an incrementing 1-byte counter on GA 0/0/1 (GO #1) every
 * SEND_INTERVAL_MS milliseconds.  Counter wraps 0-255 continuously.
 *
 * Compatible with ping_pong.py in mode 2 (ping-pong): the PC script
 * listens on KNX_RX_GA=0/0/1 and prints every telegram it receives.
 *
 * Individual addresses do NOT matter for group communication:
 *   OpenPLC  1.1.1  sends  GroupValueWrite -> GA 0/0/1
 *   KNX/USB  15.15.15 receives it - no coupler / no filter needed
 * as long as both are on the same TP bus segment.
 *
 * MASK: 0x07B0 (TP-only)
 */
#define MASK_VERSION 0x07B0u
#include <OpenPLC_KNX.h>
#include "knx/table_object.h"
#include "knx/interface_object.h"
#include "knx/property.h"

#define DPT_COUNTER      DPT_Value_1_Ucount
#define SEND_INTERVAL_MS 5000u   /* transmit once every 5 seconds */

/* -------------------------------------------------------------------------
 * selfLoadTable / selfProgramPingPong
 * Programs GO #1 -> GA 0/0/1 (TX) into NVM.
 * Skips automatically on subsequent boots if tables are already loaded.
 * ---------------------------------------------------------------------- */
static bool selfLoadTable(InterfaceObject* obj, const uint8_t* src, uint32_t n)
{
    if (!obj || !src || !n) return false;
    uint8_t buf[10];
    uint8_t cnt;

    buf[0] = 1u; cnt = 1u;
    obj->writeProperty(PID_LOAD_STATE_CONTROL, 1u, buf, cnt);

    buf[0]=3u; buf[1]=0x0Bu; buf[2]=0u; buf[3]=0u;
    buf[4]=0u; buf[5]=(uint8_t)n; buf[6]=0u; buf[7]=0u;
    cnt = 1u;
    obj->writeProperty(PID_LOAD_STATE_CONTROL, 1u, buf, cnt);

    TableObject* tbl = static_cast<TableObject*>(obj);
    uint8_t* dest = tbl->tableData();
    if (!dest) return false;
    memcpy(dest, src, n);

    buf[0] = 2u; cnt = 1u;
    obj->writeProperty(PID_LOAD_STATE_CONTROL, 1u, buf, cnt);
    return true;
}

static bool selfProgramPingPong()
{
    InterfaceObject* addrObj = KNX.getInterfaceObject(1u);
    if (!addrObj) return false;

    uint8_t state = 0u, cnt = 1u;
    addrObj->readProperty(PID_LOAD_STATE_CONTROL, 1u, cnt, &state);
    if (state == 1u) {
        Serial_Test.println("selfProgram: tables already loaded, skipping.");
        return true;
    }

    KNX.individualAddress(0x1101u);   /* 1.1.1 */

    static const uint8_t addrData[] = {
        0x00u, 0x02u,
        0x00u, 0x01u,   /* GA 0/0/1 */
        0x00u, 0x02u    /* GA 0/0/2 */
    };
    if (!selfLoadTable(KNX.getInterfaceObject(1u), addrData, sizeof(addrData))) return false;

    static const uint8_t assocData[] = {
        0x00u, 0x02u,
        0x00u, 0x01u, 0x00u, 0x01u,   /* TSAP1 -> GO #1 */
        0x00u, 0x02u, 0x00u, 0x02u    /* TSAP2 -> GO #2 */
    };
    if (!selfLoadTable(KNX.getInterfaceObject(2u), assocData, sizeof(assocData))) return false;

    static const uint8_t goData[] = {
        0x00u, 0x02u,
        0x17u, 0x07u,   /* GO #1 */
        0x17u, 0x07u    /* GO #2 */
    };
    if (!selfLoadTable(KNX.getInterfaceObject(3u), goData, sizeof(goData))) return false;

    static const uint8_t appData[] = { 0x00u };
    if (!selfLoadTable(KNX.getInterfaceObject(4u), appData, sizeof(appData))) return false;

    KNX.writeMemory();
    return true;
}

static uint8_t  s_counter    = 0u;
static uint32_t s_lastSend   = 0u;
static uint32_t s_txCount    = 0u;
static uint32_t s_dropCount  = 0u;
static bool     s_prevConn   = false;
static bool     s_prevProg   = false;
static uint32_t s_lastStatus = 0u;

void setup()
{
    Serial_Test.begin(115200);
    delay(500);
    Serial_Test.println("=== KNX TP Continuous Sender ===");

    KNXHelper.setup("OPENPLC_SENDER");
    KNXHelper.initRelayProfile2CH();

    Serial_Test.print("TP bus-OK : ");
    Serial_Test.println(KNXHelper.tpBusOk() ? "YES - bus detected" : "NO  - check wiring");
    Serial_Test.print("TP VCC-OK : ");
    Serial_Test.println(KNXHelper.tpVccOk() ? "YES - bus powered"  : "NO  - check PSU");

    bool ok = selfProgramPingPong();
    Serial_Test.print("selfProgram: ");
    Serial_Test.println(ok ? "OK" : "FAILED");

    KNXHelper.start();

    bool conn = KNX.bau().enabled();
    Serial_Test.print("STKNX IC   : ");
    Serial_Test.println(conn ? "CONNECTED - telegrams will be sent"
                              : "DISCONNECTED - telegrams dropped, check UART/bus wiring");
    s_prevConn = conn;

    Serial_Test.print("Individual address: 0x");
    Serial_Test.println(KNX.individualAddress(), HEX);
    Serial_Test.print("Sending on GA 0/0/1 every ");
    Serial_Test.print(SEND_INTERVAL_MS);
    Serial_Test.println(" ms...");
    Serial_Test.println("--------------------------------------------------");

    s_lastSend   = millis();
    s_lastStatus = millis();
    s_prevProg   = KNX.progMode();
}

void loop()
{
    KNXHelper.loop();

    /* Print programming mode state changes */
    bool prog = KNX.progMode();
    if (prog != s_prevProg) {
        s_prevProg = prog;
        if (prog) {
            Serial_Test.println(">>> PROG MODE ON  - IA 1.1.1 visible to ETS, LED ON");
        } else {
            Serial_Test.println(">>> PROG MODE OFF - normal operation, LED OFF");
        }
    }

    /* Print connection state changes immediately */
    bool conn = KNX.bau().enabled();
    if (conn != s_prevConn) {
        Serial_Test.println(conn ? ">>> STKNX IC connected  - TX active"
                                 : ">>> STKNX IC disconnected - TX dropped");
        s_prevConn = conn;
    }

    /* Periodic status line every 5 s when disconnected so the user knows */
    if (!conn && (millis() - s_lastStatus) >= 5000u) {
        s_lastStatus = millis();
        Serial_Test.print("[STATUS] STKNX disconnected, dropped=");
        Serial_Test.print(s_dropCount);
        Serial_Test.println("  - check TP+/TP- wiring and bus power supply (29 V)");
    }

    if ((millis() - s_lastSend) >= SEND_INTERVAL_MS) {
        s_lastSend = millis();

        if (!conn) {
            s_dropCount++;
            return;   /* no point sending while IC is disconnected */
        }

        s_txCount++;
        KNX.getGroupObject(1).value(KNXValue(s_counter), DPT_COUNTER);

        Serial_Test.print("[TX #");
        Serial_Test.print(s_txCount);
        Serial_Test.print("] GA 0/0/1 = 0x");
        if (s_counter < 0x10) Serial_Test.print("0");
        Serial_Test.print(s_counter, HEX);
        Serial_Test.print("  (");
        Serial_Test.print(s_counter);
        Serial_Test.println(")");

        s_counter++;   /* wraps 0xFF -> 0x00 automatically */
    }
}
