#pragma once

/*
 * stknx_data_link_layer.h - the stack's TP1 data link layer for a bare STKNX
 * transceiver. Replaces TpUartDataLinkLayer, which speaks the host protocol
 * of a TP-UART chip this board does not have.
 * $PROD/docs/modules/M3/KNX-TP-DATA-LINK.md
 */

#include "stm32h743_openplc_platform.h"
#include "knx/config.h"
#ifdef USE_TP

#include "knx/data_link_layer.h"
#include "knx/tp_frame.h"
#include "stknx_tp1.h"

/* Asks the stack whether a frame on the bus is ours to acknowledge. The name
 * is the upstream stack's, kept so the BAU classes need no change. */
class ITpUartCallBacks
{
    public:
        virtual ~ITpUartCallBacks() = default;
        virtual TPAckType isAckRequired(uint16_t address, bool isGrpAddr) = 0;
};

/* A frame seen on the bus, check octet included; forUs = acknowledged by this device. */
typedef void (*StknxBusFrameFn)(const uint8_t* frame, uint16_t len, bool forUs);
typedef void (*StknxSendDoneFn)(bool ok);

#ifndef STKNX_TX_QUEUE
#  define STKNX_TX_QUEUE  8u
#endif

class StknxDataLinkLayer : public DataLinkLayer
{
    public:
        StknxDataLinkLayer(DeviceObject& devObj, NetworkLayerEntity& netLayerEntity,
                           Platform& platform, ITpUartCallBacks& cb,
                           DataLinkLayerCallbacks* dllcb = nullptr);

        void loop() override;
        void enabled(bool value) override;
        bool enabled() const override;
        DptMedium mediumType() const override;

        /* The board has one STKNX, so one instance whatever the role. */
        static StknxDataLinkLayer* instance() { return _instance; }

        /* Hand every good frame on the bus to fn; nullptr stops. */
        void monitor(StknxBusFrameFn fn);
        /* Send a complete TP frame (check octet included) outside the stack;
         * done reports the outcome. false = one is still waiting or the link is off. */
        bool sendRaw(const uint8_t* frame, uint16_t len, StknxSendDoneFn done);
        uint16_t individualAddress() { return _deviceObject.individualAddress(); }

    protected:
        bool sendFrame(CemiFrame& frame) override;

    private:
        static uint8_t addressed(void* ctx, uint16_t dst, uint8_t isGroup);
        void confirm(TpFrame* frame, bool ok);

        ITpUartCallBacks&       _cb;
        DataLinkLayerCallbacks* _dllcb;
        bool                    _enabled = false;

        TpFrame* _txQueue[STKNX_TX_QUEUE] = {};
        uint8_t  _txHead = 0;
        uint8_t  _txCount = 0;
        TpFrame* _txInFlight = nullptr;

        StknxBusFrameFn _monitorFn = nullptr;
        uint8_t         _raw[STKNX_FRAME_MAX];
        uint16_t        _rawLen = 0;
        bool            _rawInFlight = false;
        StknxSendDoneFn _rawDone = nullptr;

        stknx_link_t _link;

        static StknxDataLinkLayer* _instance;
};

#endif
