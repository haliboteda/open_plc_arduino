#pragma once

#include <Arduino.h>
#include "knx_config.h"
#include "knx_address.h"
#include "knx_telegram.h"
#include "knx_dpt.h"
#include "knx_tp.h"
#include "knx_ip.h"

/*
 * OpenPLC_KNX — Arduino API for KNX communication on OpenPLC boards.
 *
 * Supports two transport modes:
 *   KNX_TRANSPORT_TP  — KNX Twisted-Pair via STKNX transceiver (USART1)
 *   KNX_TRANSPORT_IP  — KNXnet/IP Routing via Ethernet (LwIP / OpenPLC_Net)
 *
 * Quick start:
 *
 *   #include <OpenPLC_KNX.h>
 *
 *   void onSwitch(const KnxGroupObject *go) {
 *       bool val = dpt1_decode(go->value, go->value_len);
 *       digitalWrite(LED, val);
 *   }
 *
 *   void setup() {
 *       KNX.beginTP(knxIA(1, 1, 5));
 *       KNX.addGroupObject(knxGA(0,0,1), 1, 1, onSwitch);
 *   }
 *
 *   void loop() {
 *       KNX.process();
 *       // ... application code ...
 *   }
 *
 * Group object callbacks are invoked from within KNX.process().
 */

/* Transport type selector */
typedef enum {
    KNX_TRANSPORT_TP  = 0,  /* KNX TP via STKNX / USART1 */
    KNX_TRANSPORT_IP  = 1,  /* KNXnet/IP Routing via Ethernet */
    KNX_TRANSPORT_BOTH = 2  /* Bridge mode: receive from both, send to both */
} KnxTransport;

/*
 * A registered group object.
 *
 * One group object = one group address + optional write callback.
 * The library maintains up to KNX_MAX_GROUP_OBJECTS objects.
 * Use KNX.addGroupObject() to register; the returned pointer is stable for
 * the lifetime of the sketch.
 */
struct KnxGroupObject {
    KnxGroupAddr  address;                      /* Group address this object listens to */
    uint8_t       dpt_main;                     /* DPT main type (1, 5, 9, 14, …) */
    uint8_t       dpt_sub;                      /* DPT sub type (e.g. 1 for DPT-1.001) */
    uint8_t       value[KNX_APDU_MAX_DATA_LEN]; /* Last received / sent raw DPT value */
    uint8_t       value_len;                    /* Number of valid bytes in value[] */
    void        (*on_write)(const KnxGroupObject *go);  /* Called on GroupValue.Write */
    void        (*on_read) (const KnxGroupObject *go);  /* Called on GroupValue.Read (optional) */
};

class OpenPLC_KNX_Class {
public:
    OpenPLC_KNX_Class();

    /* --- Initialization -------------------------------------------------- */

    /* Start the TP transport.  own_addr is this device's KNX individual address.
     * Call in setup() before any other KNX method. */
    bool beginTP(KnxIndividualAddr own_addr);

    /* Start the KNXnet/IP Routing transport.
     * Requires openplc_net_init() to have been called and a DHCP address to be
     * available before this function is called.  Returns false if LwIP is not
     * ready.  Retry from the main loop until it returns true. */
    bool beginIP(KnxIndividualAddr own_addr);

    /* --- Group object registration --------------------------------------- */

    /* Register a group address.  Returns a pointer to the stored group object,
     * or NULL if the table is full (KNX_MAX_GROUP_OBJECTS).
     * on_write is called whenever a GroupValue.Write telegram is received for
     * this address.  on_read (optional) is called for GroupValue.Read; if NULL,
     * no automatic response is sent. */
    KnxGroupObject *addGroupObject(KnxGroupAddr ga,
                                   uint8_t dpt_main, uint8_t dpt_sub,
                                   void (*on_write)(const KnxGroupObject *go),
                                   void (*on_read )(const KnxGroupObject *go) = NULL);

    /* --- Group communication --------------------------------------------- */

    /* Send GroupValue.Write for a DPT-1 (boolean) value. */
    bool groupWrite(KnxGroupAddr ga, bool value);

    /* Send GroupValue.Write for a DPT-5 (uint8) value. */
    bool groupWrite(KnxGroupAddr ga, uint8_t value);

    /* Send GroupValue.Write for a DPT-9 (KNX 2-byte float) value. */
    bool groupWrite(KnxGroupAddr ga, float value);

    /* Send GroupValue.Write with a raw DPT payload (any length). */
    bool groupWriteRaw(KnxGroupAddr ga, const uint8_t *data, uint8_t len);

    /* Send GroupValue.Read. */
    bool groupRead(KnxGroupAddr ga);

    /* --- Main loop ------------------------------------------------------- */

    /* Must be called every iteration of loop().
     * Drives the TP UART receiver, programming key debounce, and the
     * programming mode LED state machine. */
    void process(void);

    /* --- Programming mode ----------------------------------------------- */

    /* Returns true if the device is currently in KNX programming mode.
     * In programming mode the KNX_PROG_LED blinks and ETS can assign a new
     * individual address over the bus. */
    bool isProgMode(void) const;

    /* Forcibly set programming mode on or off. */
    void setProgMode(bool on);

    /* --- Status ---------------------------------------------------------- */

    KnxTransport      transport(void) const { return _transport; }
    KnxIndividualAddr ownAddress(void) const { return _own_addr; }
    bool              tpBusOk(void)      const;  /* KNX_TP_OK_PIN (bus powered) */
    bool              tpConnected(void) const;  /* chip responded to reset */
    bool              ipReady(void)  const;

private:
    /* Dispatch a received telegram to registered group objects */
    void dispatch(const KnxTelegram *tg);

    /* Internal send helper that routes to the active transport(s) */
    bool send(KnxTelegram *tg);

    /* Static C callbacks forwarded to the instance */
    static void s_tp_rx(const KnxTelegram *tg);
    static void s_ip_rx(const KnxTelegram *tg);

    KnxTransport      _transport;
    KnxIndividualAddr _own_addr;
    bool              _prog_mode;
    uint32_t          _prog_led_last_ms;    /* Last LED toggle timestamp */

    KnxGroupObject    _objects[KNX_MAX_GROUP_OBJECTS];
    uint8_t           _obj_count;
};

/* Singleton instance — use as KNX.beginTP(...), KNX.process(), etc. */
extern OpenPLC_KNX_Class KNX;
