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
#include "knx/tpuart_data_link_layer.h"   /* ITpUartCallBacks, TpFrame */
#include "stknx_tp1.h"

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

        stknx_link_t _link;
};

#endif
