/*
 * KNX_Basic - Minimal OpenPLC_KNX usage example.
 *
 * Default MASK_VERSION is 0x5780 (IP+TP dual device): the board communicates
 * on BOTH KNXnet/IP (Ethernet) and KNX TP bus simultaneously.  Group object
 * callbacks fire for writes arriving from either medium.  Outgoing writes
 * (e.g. go.objectWritten()) are broadcast on both IP and TP.
 *
 * Before first use, program the device with ETS:
 *   1. Press the button on PG9 to enter programming mode (LED on PG11 lights).
 *   2. In ETS, assign the individual address and download the application
 *      (ETS connects over KNXnet/IP on the Ethernet port).
 *   3. ETS maps group addresses to group objects - the indices used below
 *      must match the group object table in your ETS application design.
 *
 * Default wiring (Bridge MPU schematic):
 *   Prog button : PG9    (EXTI9_5, pull-up, active-low)
 *   Prog LED    : PG11   (active-high)
 *   Relay 0     : PE6    (REL_1, coil energised = GPIO_PIN_SET)
 *   Relay 1     : PE5    (REL_2, coil energised = GPIO_PIN_SET)
 *   KNX TP TX   : PB14   (USART1 AF4)
 *   KNX TP RX   : PA10   (USART1 AF7)
 *
 * Group object table (configure the same layout in ETS):
 *   GO index 1  DPT-1.001 (1-bit switch) - controls relay channel 0 (PE6)
 *   GO index 2  DPT-1.001 (1-bit switch) - controls relay channel 1 (PE5)
 */

#include <OpenPLC_KNX.h>

/* -------------------------------------------------------------------------
 * Group object callbacks - invoked by the KNX stack on GroupValue.Write.
 * The GO index used in getGroupObject() must match the ETS project.
 * ---------------------------------------------------------------------- */

static void onRelay0(GroupObject &go)
{
    bool on = (bool)go.value(DPT_Switch);
    KNXHelper.setRelayChannel(0, on);
    /* Optionally acknowledge back to the bus (status feedback): */
    go.objectWritten();
}

static void onRelay1(GroupObject &go)
{
    bool on = (bool)go.value(DPT_Switch);
    KNXHelper.setRelayChannel(1, on);
    go.objectWritten();
}

/* -------------------------------------------------------------------------
 * setup()
 * ---------------------------------------------------------------------- */
void setup()
{
    /* 1. Initialise KNX stack: loads Flash NVM, configures prog-LED on PG11
     *    and prog-button EXTI on PG9, sets serial number visible in ETS. */
    KNXHelper.setup("OPENPLC000001");

    /* 2. Initialise 2-channel relay profile: configures PE6 / PE5 as GPIO
     *    outputs and restores the last relay states from NVM. */
    KNXHelper.initRelayProfile2CH();

    /* 3. Register group-object callbacks.
     *    configured() is false until ETS has downloaded an application.
     *    On a freshly-programmed board the callbacks are always registered. */
    if (KNX.configured()) {
        KNX.getGroupObject(1).callback(onRelay0);
        KNX.getGroupObject(2).callback(onRelay1);
    }

    /* 4. Enable the transport(s) - must be called after all GO registrations. */
    KNXHelper.start();
}

/* -------------------------------------------------------------------------
 * loop()
 * ---------------------------------------------------------------------- */
void loop()
{
    /* Drive the KNX stack: receive frames, run timers, handle prog-mode FSM. */
    KNXHelper.loop();
}
