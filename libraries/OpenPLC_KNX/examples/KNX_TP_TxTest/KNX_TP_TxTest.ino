/*
 * KNX_TP_TxTest — Periodic TP bus transmit test for oscilloscope verification.
 *
 * Default MASK_VERSION is 0x5780 (IP+TP dual device) — both Ethernet and TP
 * bus are active.  The TP bus signal is visible on an oscilloscope at TP+/TP-.
 * ETS can also observe the group telegrams on the IP side simultaneously.
 *
 * Does NOT require ETS or any other KNX device on the bus.
 * Uses selfProgram2CH() to assign default group addresses without ETS:
 *   GO #1  →  GA 0/0/1   (DPT-1.001 switch, toggled every TX_INTERVAL_MS)
 *   GO #2  →  GA 0/0/2   (DPT-1.001 switch, toggled every TX_INTERVAL_MS, inverted)
 *
 * Oscilloscope setup (e.g. Hantek DSO2D15):
 *   Probe     : CH1, 10x, probe tip on TP+, clip on TP-
 *   V/div     : 5 V/div   (shows full 0–29 V swing in 8 divisions)
 *   Time/div  : 1 ms/div  (shows a full KNX telegram per trigger)
 *   Coupling  : DC
 *   Trigger   : Edge, CH1, Falling, Normal, level ~15 V
 *
 * Serial_Test output (115200 baud) confirms each telegram sent.
 * TP bus-OK and VCC-OK GPIO states are printed on startup.
 *
 * NOTE: MASK_VERSION is overridden to 0x07B0 (TP-only) here because this
 * sketch tests the TP bus signal with an oscilloscope and does not need
 * Ethernet/IP.  The 0x57B0/0x5780 IP stack adds ~49 KB of static RAM
 * (LwIP buffers) that would leave <17 KB free and trigger stability warnings.
 * Remove the override and add openplc_net_init() when IP is also needed.
 */
#define MASK_VERSION 0x07B0u   /* TP-only for this oscilloscope test */
#include <OpenPLC_KNX.h>

#define TX_INTERVAL_MS  2000u   /* send every 2 seconds */

static uint32_t s_lastTx = 0;
static bool     s_state  = false;

/* -------------------------------------------------------------------------
 * setup
 * ---------------------------------------------------------------------- */
void setup()
{
    Serial_Test.begin(115200);
    delay(500);
    Serial_Test.println("=== KNX TP Transmit Test (MASK 0x5780 — IP+TP dual) ===");

    /* Erase KNX stack NVM (Flash Bank2 Sector6, 0x081C0000) before setup().
     * Required after a MASK_VERSION change: old table data causes a crash
     * inside KNXHelper.setup() during restoration. */
    {
        Serial_Test.print("Erasing stack NVM (Sector 6)... ");
        FLASH_EraseInitTypeDef e;
        e.TypeErase = FLASH_TYPEERASE_SECTORS;
        e.Banks     = FLASH_BANK_2;
        e.Sector    = FLASH_SECTOR_6;
        e.NbSectors = 1u;
        uint32_t err = 0u;
        HAL_FLASH_Unlock();
        HAL_FLASHEx_Erase(&e, &err);
        HAL_FLASH_Lock();
        Serial_Test.println(err == 0xFFFFFFFFu ? "OK" : "FAILED");
    }

    /* 1. Core init: NVM load, prog-LED (PG11), prog-button EXTI (PG9). */
    KNXHelper.setup("OPENPLCTXTEST1");

    /* 2. Relay GPIO init (required by selfProgram2CH internally). */
    KNXHelper.initRelayProfile2CH();

    /* 3. Check bus hardware before starting the stack. */
    Serial_Test.print("TP bus-OK  (PD7) : ");
    Serial_Test.println(KNXHelper.tpBusOk()  ? "HIGH — bus detected"  : "LOW  — no bus signal");
    Serial_Test.print("TP VCC-OK (PH12) : ");
    Serial_Test.println(KNXHelper.tpVccOk() ? "HIGH — bus powered"   : "LOW  — no bus power");

    /* 4. Self-program without ETS. Sector 6 was just erased so
     *    selfProgram2CH() will always write fresh tables. */
    bool ok = KNXHelper.selfProgram2CH();
    Serial_Test.print("selfProgram2CH() : ");
    Serial_Test.println(ok ? "OK" : "FAILED — check Flash/NVM");

    /* 5. Enable the TP transport.  Must come after table setup. */
    KNXHelper.start();

    Serial_Test.print("Individual addr  : 0x");
    Serial_Test.println(KNX.individualAddress(), HEX);
    Serial_Test.print("Configured       : ");
    Serial_Test.println(KNX.configured() ? "yes" : "no — telegrams may not transmit");
    Serial_Test.println();
    Serial_Test.print("Sending GO1/GO2 toggle every ");
    Serial_Test.print(TX_INTERVAL_MS);
    Serial_Test.println(" ms on BOTH TP bus and IP multicast.  Trigger scope now.");
    Serial_Test.println("--------------------------------------------------");
}

/* -------------------------------------------------------------------------
 * loop
 * ---------------------------------------------------------------------- */
void loop()
{
    /* Must be called on every iteration — drives RX, timers, prog-mode FSM. */
    KNXHelper.loop();

    if (millis() - s_lastTx >= TX_INTERVAL_MS) {
        s_lastTx = millis();
        s_state  = !s_state;

        /* Send on GO #1 (GA 0/0/1) */
        GroupObject &go1 = KNX.getGroupObject(1);
        go1.value(KNXValue(s_state), DPT_Switch);

        /* Send on GO #2 (GA 0/0/2) — inverted, gives alternating pattern */
        GroupObject &go2 = KNX.getGroupObject(2);
        go2.value(KNXValue(!s_state), DPT_Switch);

        Serial_Test.print("[");
        Serial_Test.print(millis());
        Serial_Test.print(" ms]  TX  GO1=");
        Serial_Test.print(s_state ? "ON " : "OFF");
        Serial_Test.print("  GO2=");
        Serial_Test.println(!s_state ? "ON " : "OFF");
    }
}
