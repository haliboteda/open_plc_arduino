#pragma once

/*
 * OpenPLC_KNX.h - Public API for KNX communication on OpenPLC STM32H743 boards.
 *
 * The sketch uses one object, KNX. It wraps the thelsing/knx stack; the
 * sketch's OPENPLC_KNX_ROLE() line picks the mask (TP_DEVICE: 0x07B0,
 * IP_ROUTER: 0x091A).
 *
 * Usage (Arduino sketch):
 *
 *   OPENPLC_APP_VERSION(1, 0, 0);
 *   OPENPLC_KNX_ROLE(TP_DEVICE);
 *
 *   #include <OpenPLC_KNX.h>
 *
 *   void onSwitch(GroupObject& go) {
 *       bool on = go.value<bool>();
 *       digitalWrite(LED_BUILTIN, on);
 *   }
 *
 *   void setup() {
 *       KNX.setup(OPENPLC_KNX_ORDER_NUMBER);   // order number of the ETS product
 *       if (KNX.configured()) {
 *           KNX.getGroupObject(KNX_OBJ_RELAY1_SWITCH).callback(onSwitch);
 *       }
 *       KNX.start();
 *   }
 *
 *   void loop() {
 *       KNX.loop();
 *   }
 *
 * OPENPLC_KNX_ORDER_NUMBER and the KNX_OBJ_* object numbers come from
 * openplc_knx_objects.h, generated from the ETS product (included below).
 *
 * Programming mode button / LED are handled internally (HAL EXTI + GPIO).
 * Press the button on PG9 to toggle ETS programming mode.
 */

/* Pull in KNX_NO_AUTOMATIC_GLOBAL_INSTANCE (and the missing-role check)
 * before any reference-library header. */
#include "stm32h743_openplc_platform.h"

/* Reference library public headers */
#include <knx.h>                        /* GroupObject, DPT types       */
#include <knx/group_object.h>           /* GroupObject class            */

/* Only masks ETS knows (decision 95):
 *   0x07B0  KNX TP device - group objects, KNX.getGroupObject(n)
 *   0x091A  KNX IP router           - routes between IP and TP, no group objects */
#if MASK_VERSION != 0x07B0 && MASK_VERSION != 0x091A
  #error "OpenPLC_KNX: unsupported MASK_VERSION. Use OPENPLC_KNX_ROLE(TP_DEVICE); or OPENPLC_KNX_ROLE(IP_ROUTER);"
#endif

/* Order number and group object numbers of the ETS product (generated; edit knx_objects.json) */
#include "openplc_knx_objects.h"

/* -------------------------------------------------------------------------
 * One frame seen on the TP bus, for KNX.onBusFrame().
 * ---------------------------------------------------------------------- */
struct KnxBusFrame
{
    uint16_t       source;        /* individual address of the sender      */
    uint16_t       destination;   /* group or individual address           */
    bool           group;         /* destination is a group address        */
    bool           forUs;         /* this board acknowledged it            */
    const uint8_t* tpdu;          /* TPCI, APCI and data                   */
    uint8_t        tpduLen;
    const uint8_t* raw;           /* the whole frame, check octet included */
    uint16_t       rawLen;
};

typedef void (*KnxBusFrameCallback)(const KnxBusFrame& frame);
typedef void (*KnxBusSendDoneCallback)(bool ok);

/* -------------------------------------------------------------------------
 * The sketch's only KNX object (decision 109): board set-up, the stack,
 * group objects and the TP bus outside the stack.
 * ---------------------------------------------------------------------- */
class OpenPLC_KNX_Class
{
public:
    /* Call once in setup().
     *   serial     - order number of the ETS product, first 10 characters used (e.g. "OPENPLC-TP")
     *   mfr_id     - KNX manufacturer ID (default 0x00FA: listed in ETS as "KNX Association", the shared test ID)
     * Drives KNX_TX (PB14) low, configures the prog-button interrupt on PG9
     * and the prog-mode LED (the system LED, PE2), loads the ETS data.
     * Does NOT start the stack - call start() after registering group objects. */
    void setup(const char* serial = "OPENPLC-TP",
               uint16_t    mfr_id = 0x00FAu);

    /* Enables the stack and transports; call after setting group-object callbacks. */
    void start();

    /* Call every iteration of loop(). */
    void loop();

    /* --- Status -------------------------------------------------------- */
    bool     progMode() const;
    bool     configured() const;          /* ETS has downloaded an application */
    uint16_t individualAddress() const;

#if MASK_VERSION == 0x07B0
    /* Group object n as numbered in the ETS product. Only a TP device has them. */
    GroupObject& getGroupObject(uint16_t n);
#endif

    /* --- TP bus, outside the stack (no ETS configuration needed) ------- */
    /* Every good frame on the TP bus goes to cb, whoever it is for; nullptr
     * stops. Called from loop(). */
    void onBusFrame(KnxBusFrameCallback cb);
    /* GroupValue.Write onto the TP bus from this board's individual address,
     * after start(). The first form packs a 0..63 value into the APCI (DPT 1,
     * 2, 3); the second appends 1..14 data octets. false = a previous one is
     * still going out. The outcome goes to onBusSendDone(): ok = acknowledged. */
    bool sendGroupWrite(uint16_t groupAddress, uint8_t smallValue);
    bool sendGroupWrite(uint16_t groupAddress, const uint8_t* data, uint8_t len);
    void onBusSendDone(KnxBusSendDoneCallback cb);
};

extern OpenPLC_KNX_Class KNX;
