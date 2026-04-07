#include "OpenPLC_KNX.h"
#include <string.h>

/* Singleton definition */
OpenPLC_KNX_Class KNX;

/* Static pointer to the singleton instance, used by C callbacks */
static OpenPLC_KNX_Class *s_instance = NULL;

/* -------------------------------------------------------------------------
 * Constructor
 * ---------------------------------------------------------------------- */

OpenPLC_KNX_Class::OpenPLC_KNX_Class()
    : _transport(KNX_TRANSPORT_TP)
    , _own_addr(knxIA(1, 1, 1))
    , _prog_mode(false)
    , _prog_led_last_ms(0u)
    , _obj_count(0u)
{
    memset(_objects, 0, sizeof(_objects));
    s_instance = this;
}

/* -------------------------------------------------------------------------
 * Initialization
 * ---------------------------------------------------------------------- */

bool OpenPLC_KNX_Class::beginTP(KnxIndividualAddr own_addr)
{
    _own_addr  = own_addr;
    _transport = KNX_TRANSPORT_TP;
    if (!knx_tp_init(_own_addr)) {
        return false;
    }
    knx_tp_set_rx_callback(OpenPLC_KNX_Class::s_tp_rx);
    return true;
}

bool OpenPLC_KNX_Class::beginIP(KnxIndividualAddr own_addr)
{
    _own_addr  = own_addr;
    _transport = KNX_TRANSPORT_IP;
    if (!knx_ip_init()) {
        return false;
    }
    knx_ip_set_rx_callback(OpenPLC_KNX_Class::s_ip_rx);
    return true;
}

/* -------------------------------------------------------------------------
 * Group object registration
 * ---------------------------------------------------------------------- */

KnxGroupObject *OpenPLC_KNX_Class::addGroupObject(
    KnxGroupAddr ga,
    uint8_t dpt_main, uint8_t dpt_sub,
    void (*on_write)(const KnxGroupObject *go),
    void (*on_read )(const KnxGroupObject *go))
{
    if (_obj_count >= KNX_MAX_GROUP_OBJECTS) {
        return NULL;
    }
    KnxGroupObject *obj = &_objects[_obj_count++];
    memset(obj, 0, sizeof(KnxGroupObject));
    obj->address  = ga;
    obj->dpt_main = dpt_main;
    obj->dpt_sub  = dpt_sub;
    obj->on_write = on_write;
    obj->on_read  = on_read;
    return obj;
}

/* -------------------------------------------------------------------------
 * Group value write helpers
 * ---------------------------------------------------------------------- */

bool OpenPLC_KNX_Class::groupWrite(KnxGroupAddr ga, bool value)
{
    uint8_t raw[1];
    dpt1_encode(value, raw);
    return groupWriteRaw(ga, raw, 1u);
}

bool OpenPLC_KNX_Class::groupWrite(KnxGroupAddr ga, uint8_t value)
{
    uint8_t raw[1];
    dpt5_encode(value, raw);
    return groupWriteRaw(ga, raw, 1u);
}

bool OpenPLC_KNX_Class::groupWrite(KnxGroupAddr ga, float value)
{
    uint8_t raw[2];
    dpt9_encode(value, raw);
    return groupWriteRaw(ga, raw, 2u);
}

bool OpenPLC_KNX_Class::groupWriteRaw(KnxGroupAddr ga,
                                       const uint8_t *data, uint8_t len)
{
    KnxTelegram tg;
    knxTelegram_buildGroupWrite(&tg, _own_addr, ga, data, len);
    return send(&tg);
}

bool OpenPLC_KNX_Class::groupRead(KnxGroupAddr ga)
{
    KnxTelegram tg;
    knxTelegram_buildGroupRead(&tg, _own_addr, ga);
    return send(&tg);
}

/* -------------------------------------------------------------------------
 * Internal send — route to active transport(s)
 * ---------------------------------------------------------------------- */

bool OpenPLC_KNX_Class::send(KnxTelegram *tg)
{
    bool ok = false;
    if (_transport == KNX_TRANSPORT_TP || _transport == KNX_TRANSPORT_BOTH) {
        ok |= knx_tp_send(tg);
    }
    if (_transport == KNX_TRANSPORT_IP || _transport == KNX_TRANSPORT_BOTH) {
        ok |= knx_ip_send(tg);
    }
    return ok;
}

/* -------------------------------------------------------------------------
 * Telegram dispatch — match received telegram to registered group objects
 * ---------------------------------------------------------------------- */

void OpenPLC_KNX_Class::dispatch(const KnxTelegram *tg)
{
    if (!tg->dst_is_group) {
        /* Individual-addressed frames (e.g., ETS programming) are not
         * dispatched to group objects.  Future: handle BCU address assignment. */
        return;
    }

    KnxService svc = knxTelegram_service(tg);

    for (uint8_t i = 0u; i < _obj_count; i++) {
        KnxGroupObject *obj = &_objects[i];
        if (obj->address != tg->dst) {
            continue;
        }

        if (svc == KNX_SVC_GROUP_WRITE || svc == KNX_SVC_GROUP_RESP) {
            /* Update the cached value */
            uint8_t data_len;
            const uint8_t *data = knxTelegram_data(tg, &data_len);
            if (data != NULL && data_len <= KNX_APDU_MAX_DATA_LEN) {
                memcpy(obj->value, data, data_len);
                obj->value_len = data_len;
            }
            if (svc == KNX_SVC_GROUP_WRITE && obj->on_write != NULL) {
                obj->on_write(obj);
            }
        } else if (svc == KNX_SVC_GROUP_READ) {
            if (obj->on_read != NULL) {
                /* The on_read callback is responsible for sending a GroupValue.Response
                 * if desired (e.g., call KNX.groupWriteRaw() or use the cached value). */
                obj->on_read(obj);
            }
        }
    }
}

/* -------------------------------------------------------------------------
 * Main loop — process()
 * ---------------------------------------------------------------------- */

void OpenPLC_KNX_Class::process(void)
{
    /* Drive the TP receiver (reads UART bytes, assembles frames) */
    if (_transport == KNX_TRANSPORT_TP || _transport == KNX_TRANSPORT_BOTH) {
        knx_tp_poll();
    }

    /* Programming mode: toggle LED at 1 Hz if active, off if inactive */
    if (_prog_mode) {
        if ((millis() - _prog_led_last_ms) >= 500u) {
            _prog_led_last_ms = millis();
            /* Toggle: read current state and invert */
            static bool led_state = false;
            led_state = !led_state;
            knx_tp_prog_led_set(led_state);
        }

        /* Programming key: exit programming mode on a second press */
        static bool last_key = false;
        bool cur_key = knx_tp_prog_key_pressed();
        if (cur_key && !last_key) {
            /* Rising edge: toggle programming mode off */
            setProgMode(false);
        }
        last_key = cur_key;
    } else {
        /* Not in programming mode: watch for key press to enter */
        static bool last_key_idle = false;
        bool cur_key = knx_tp_prog_key_pressed();
        if (cur_key && !last_key_idle) {
            setProgMode(true);
        }
        last_key_idle = cur_key;
    }
}

/* -------------------------------------------------------------------------
 * Programming mode
 * ---------------------------------------------------------------------- */

bool OpenPLC_KNX_Class::isProgMode(void) const
{
    return _prog_mode;
}

void OpenPLC_KNX_Class::setProgMode(bool on)
{
    _prog_mode = on;
    if (!on) {
        knx_tp_prog_led_set(false);
    }
    _prog_led_last_ms = millis();
}

/* -------------------------------------------------------------------------
 * Status
 * ---------------------------------------------------------------------- */

bool OpenPLC_KNX_Class::tpBusOk(void) const
{
    return knx_tp_bus_ok();
}

bool OpenPLC_KNX_Class::tpConnected(void) const
{
    return knx_tp_connected();
}

bool OpenPLC_KNX_Class::ipReady(void) const
{
    return knx_ip_ready();
}

/* -------------------------------------------------------------------------
 * Static C callbacks (forwarded to the singleton instance)
 * ---------------------------------------------------------------------- */

void OpenPLC_KNX_Class::s_tp_rx(const KnxTelegram *tg)
{
    if (s_instance != NULL) {
        s_instance->dispatch(tg);
    }
}

void OpenPLC_KNX_Class::s_ip_rx(const KnxTelegram *tg)
{
    if (s_instance != NULL) {
        s_instance->dispatch(tg);
    }
}
