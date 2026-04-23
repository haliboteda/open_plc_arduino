/*
 * KNX_SelfTest — Board-level self-test for OpenPLC_KNX on STM32H743.
 *
 * Does NOT require a connected KNX bus or ETS.  Reports results on Serial
 * (UART4 at 115200, routed to the debug connector).
 *
 * Tests performed:
 *   1. Relay 2-channel profile init (GPIO configuration)
 *   2. Relay channel on/off control and state read-back
 *   3. Application NVM save and reload (Flash Bank2 Sector7)
 *   4. TP bus-OK and VCC-OK status GPIO reads (passive — checks pin logic)
 *   5. KNX individual address set/get round-trip
 *
 * Note: the IP stack (Ethernet + LwIP) must be initialised by the
 * application before KNX IP transport is tested.  This sketch only tests
 * the HAL-level board functions.
 */

#include <OpenPLC_KNX.h>

static void pass(const char *name) {
    Serial.print("[PASS] ");
    Serial.println(name);
}

static void fail(const char *name) {
    Serial.print("[FAIL] ");
    Serial.println(name);
}

static void check(const char *name, bool ok) {
    if (ok) pass(name); else fail(name);
}

void setup()
{
    Serial.begin(115200);
    delay(400);   /* give the host time to open the port */
    Serial.println("=== OpenPLC KNX self-test ===");

    /* ------------------------------------------------------------------
     * 1. Setup: loads NVM, inits prog-LED (PG11) and prog-button (PG9).
     * ------------------------------------------------------------------ */
    KNXHelper.setup("OPENPLCST0001");

    /* ------------------------------------------------------------------
     * 2. Relay profile init.
     * ------------------------------------------------------------------ */
    bool ok = KNXHelper.initRelayProfile2CH();
    check("Relay profile init", ok);

    /* ------------------------------------------------------------------
     * 3. Relay channel 0: on → verify → off → verify.
     * ------------------------------------------------------------------ */
    KNXHelper.setRelayChannel(0, true);
    check("Relay 0 ON",  KNXHelper.relayChannelState(0) == true);
    KNXHelper.setRelayChannel(0, false);
    check("Relay 0 OFF", KNXHelper.relayChannelState(0) == false);

    /* ------------------------------------------------------------------
     * 4. Relay channel 1: on → verify → off → verify.
     * ------------------------------------------------------------------ */
    KNXHelper.setRelayChannel(1, true);
    check("Relay 1 ON",  KNXHelper.relayChannelState(1) == true);
    KNXHelper.setRelayChannel(1, false);
    check("Relay 1 OFF", KNXHelper.relayChannelState(1) == false);

    /* ------------------------------------------------------------------
     * 5. Application NVM: save then reload and verify individual address.
     * ------------------------------------------------------------------ */
    /* Set a known individual address (1.1.5 = 0x1105) */
    KNX.individualAddress(0x1105u);
    ok = KNXHelper.saveAppConfig();
    check("App NVM save", ok);

    /* Reload and check */
    ok = KNXHelper.loadAppConfig();
    check("App NVM reload", ok);
    check("Individual address round-trip",
          KNXHelper.appConfig()->individual_addr == 0x1105u);

    /* ------------------------------------------------------------------
     * 6. Status GPIOs — passive read (bus may not be connected).
     * ------------------------------------------------------------------ */
    Serial.print("TP bus-OK pin (PD7): ");
    Serial.println(KNXHelper.tpBusOk() ? "HIGH" : "LOW");

    Serial.print("TP VCC-OK pin (PH12): ");
    Serial.println(KNXHelper.tpVccOk() ? "HIGH (bus powered)" : "LOW (no bus power)");

    /* ------------------------------------------------------------------
     * Done.
     * ------------------------------------------------------------------ */
    Serial.println("=== self-test complete ===");
}

void loop()
{
    /* Nothing — self-test runs once in setup(). */
}
