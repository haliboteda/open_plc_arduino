#pragma once

#include "config.h"
#if MASK_VERSION == 0x5780

#include "bau_systemB_device.h"
#include "ip_parameter_object.h"
#include "ip_data_link_layer.h"
#include "tpuart_data_link_layer.h"
#include "cemi_server_object.h"

/*
 * Thin wrappers that expose sendFrame() (now protected after the change in
 * ip_data_link_layer.h / tpuart_data_link_layer.h) as a public forwarder.
 * Required because DataLinkLayer::sendFrame() cannot be called across
 * unrelated instances — each class may only call sendFrame() on *itself*.
 */
class _Bau5780IpDLL : public IpDataLinkLayer
{
public:
    using IpDataLinkLayer::IpDataLinkLayer;
    bool sendFrameEx(CemiFrame& f) { return sendFrame(f); }
};

class _Bau5780TpDLL : public TpUartDataLinkLayer
{
public:
    using TpUartDataLinkLayer::TpUartDataLinkLayer;
    bool sendFrameEx(CemiFrame& f) { return sendFrame(f); }
};

/*
 * Dual-send proxy DLL — registered with NetworkLayerEntity as the outgoing DLL.
 * Every outgoing frame is forwarded to both IP and TP sub-DLLs simultaneously.
 * Receiving is handled independently by _Bau5780IpDLL and _Bau5780TpDLL,
 * each holding a reference to the same NetworkLayerEntity so frames from
 * either medium propagate up to the same application layer.
 */
class _Bau5780DualDLL : public DataLinkLayer
{
public:
    _Bau5780DualDLL(DeviceObject& devObj, NetworkLayerEntity& ne, Platform& plat,
                    _Bau5780IpDLL& ip, _Bau5780TpDLL& tp);

    void      loop()            override {}
    bool      enabled()   const override;
    void      enabled(bool v)   override;
    DptMedium mediumType() const override { return DptMedium::KNX_IP; }

protected:
    bool sendFrame(CemiFrame& frame) override;

private:
    _Bau5780IpDLL& _ip;
    _Bau5780TpDLL& _tp;
};

/*
 * Bau5780 — KNX IP+TP dual-transport application device (OpenPLC MASK 0x5780).
 *
 * Merges Bau57B0 (IP) and Bau07B0 (TP) capabilities:
 *   - Application group objects via KNX.getGroupObject() (like any app device).
 *   - Receives group telegrams from BOTH KNXnet/IP (Ethernet) and KNX TP bus.
 *   - Sends outgoing group telegrams on BOTH transports simultaneously.
 *   - ETS programs the device over IP via KNXnet/IP tunnelling.
 *
 * Architecture:
 *   _dlLayerIp and _dlLayerTp both reference the same NetworkLayerEntity,
 *   so frames received on either medium reach the same application layer.
 *   Outgoing frames are broadcast on both media via the _sendDLL proxy.
 */
class Bau5780 : public BauSystemBDevice, public ITpUartCallBacks, public DataLinkLayerCallbacks
{
public:
    explicit Bau5780(Platform& platform);

    void loop()          override;
    bool enabled()       override;
    void enabled(bool v) override;
    void syncCemiClientAddress();

    _Bau5780IpDLL* getIpDataLinkLayer();
    _Bau5780TpDLL* getTpDataLinkLayer();

    InterfaceObject* getInterfaceObject(uint8_t idx)                                    override;
    InterfaceObject* getInterfaceObject(ObjectType objectType, uint16_t objectInstance) override;

protected:
    TPAckType isAckRequired(uint16_t address, bool isGrpAddr)    override;
    void      doMasterReset(EraseCode eraseCode, uint8_t channel) override;

private:
    IpParameterObject  _ipParameters;
    _Bau5780IpDLL      _dlLayerIp;
    _Bau5780TpDLL      _dlLayerTp;
    _Bau5780DualDLL    _sendDLL;

#ifdef USE_CEMI_SERVER
    CemiServer       _cemiServer;
    CemiServerObject _cemiServerObject;
#endif
};

#endif /* MASK_VERSION == 0x5780 */
