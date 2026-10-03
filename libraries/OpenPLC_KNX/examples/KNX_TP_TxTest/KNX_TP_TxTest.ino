/*
 * KNX_TP_TxTest - Periodic TP bus transmit test for oscilloscope verification.
 *
 * The TP bus signal is visible on an oscilloscope at TP+/TP-. The sketch does
 * not start Ethernet (no openplc_net_init()), so nothing goes out over IP.
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
 * Serial_Test output (RS232, terminals C05 / C06, 115200 baud) confirms each
 * telegram sent. The KNX_OK and VCC-OK GPIO states are printed on startup.
 *
 * Role: any role with TP (Tools -> KNX Role); "KNX TP Device" leaves the IP
 * stack out. A #define MASK_VERSION in the sketch does not work: the library
 * is built with the menu's value, and the two would disagree about KNX.
 */

// Every sketch declares its own version. The upload tool compares it with
// the one on the board and refuses to flash an older one over a newer one.
OPENPLC_APP_VERSION(1, 0, 0);
#include <OpenPLC_KNX.h>

#define TX_INTERVAL_MS  2000u   /* send every 2 seconds */

static uint32_t s_lastTx = 0;
static bool     s_state  = false;

/* -------------------------------------------------------------------------
 * setup
 * ---------------------------------------------------------------------- */
void setup()
{
    // The RS232 transceiver is off after reset; turn it on or nothing reaches
    // terminals C05 / C06.
    pinMode(RS232_EN_Pin, OUTPUT);
    digitalWrite(RS232_EN_Pin, HIGH);
    Serial_Test.begin(115200);
    delay(500);
    Serial_Test.print("=== KNX TP Transmit Test (MASK 0x");
    Serial_Test.print(MASK_VERSION, HEX);
    Serial_Test.println(") ===");

    /* Erase the KNX sector (Flash Bank2 Sector6, 0x081C0000) before setup().
     * Required after a MASK_VERSION change: old table data causes a crash
     * inside KNXHelper.setup() during restoration. This also clears the
     * application NVM stored in the same sector; setup() restores defaults. */
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

    /* 1. Core init: NVM load, KNX_TX low, prog-LED line (PG11), prog-button EXTI (PG9). */
    KNXHelper.setup("OPENPLCTXTEST1");

    /* 2. Relay GPIO init (required by selfProgram2CH internally). */
    KNXHelper.initRelayProfile2CH();

    /* 3. Check bus hardware before starting the stack. */
    /* PD7 reads LOW on this board whatever the bus does; PH12 is the one
     * that follows bus power. */
    Serial_Test.print("TP KNX_OK  (PD7) : ");
    Serial_Test.println(KNXHelper.tpBusOk()  ? "HIGH" : "LOW");
    Serial_Test.print("TP VCC-OK (PH12) : ");
    Serial_Test.println(KNXHelper.tpVccOk() ? "HIGH - bus powered"   : "LOW  - no bus power");

    /* 4. Self-program without ETS. Sector 6 was just erased so
     *    selfProgram2CH() will always write fresh tables. */
    bool ok = KNXHelper.selfProgram2CH();
    Serial_Test.print("selfProgram2CH() : ");
    Serial_Test.println(ok ? "OK" : "FAILED - check Flash/NVM");

    /* 5. Enable the TP transport.  Must come after table setup. */
    KNXHelper.start();

    Serial_Test.print("Individual addr  : 0x");
    Serial_Test.println(KNX.individualAddress(), HEX);
    Serial_Test.print("Configured       : ");
    Serial_Test.println(KNX.configured() ? "yes" : "no - telegrams may not transmit");
    Serial_Test.println();
    Serial_Test.print("Sending GO1/GO2 toggle every ");
    Serial_Test.print(TX_INTERVAL_MS);
    Serial_Test.println(" ms on the TP bus.  Trigger scope now.");
    Serial_Test.println("--------------------------------------------------");
}

/* -------------------------------------------------------------------------
 * loop
 * ---------------------------------------------------------------------- */
void loop()
{
    /* Must be called on every iteration - drives RX, timers, prog-mode FSM. */
    KNXHelper.loop();

    if (millis() - s_lastTx >= TX_INTERVAL_MS) {
        s_lastTx = millis();
        s_state  = !s_state;

        /* Send on GO #1 (GA 0/0/1) */
        GroupObject &go1 = KNX.getGroupObject(1);
        go1.value(KNXValue(s_state), DPT_Switch);

        /* Send on GO #2 (GA 0/0/2) - inverted, gives alternating pattern */
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
