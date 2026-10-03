#pragma once

#include "config.h"
#if MASK_VERSION == 0x07B0

#include "bau_systemB_device.h"
#include "../stknx_data_link_layer.h"   /* OpenPLC: STKNX, not a TP-UART */
#include "cemi_server.h"
#include "cemi_server_object.h"

class Bau07B0 : public BauSystemBDevice, public ITpUartCallBacks, public DataLinkLayerCallbacks
{
    public:
        Bau07B0(Platform& platform);
        void loop() override;
        bool enabled() override;
        void enabled(bool value) override;
        void syncCemiClientAddress();

        StknxDataLinkLayer* getDataLinkLayer() { return &_dlLayer; }
        InterfaceObject* getInterfaceObject(uint8_t idx);
        InterfaceObject* getInterfaceObject(ObjectType objectType, uint16_t objectInstance);
    protected:

        // For TP1 only
        TPAckType isAckRequired(uint16_t address, bool isGrpAddr) override;

    private:
        StknxDataLinkLayer _dlLayer;
#ifdef USE_CEMI_SERVER
        CemiServer _cemiServer;
        CemiServerObject _cemiServerObject;
#endif
};
#endif
