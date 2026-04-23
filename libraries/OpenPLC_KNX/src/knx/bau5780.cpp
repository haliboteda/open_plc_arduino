#include "config.h"
#if MASK_VERSION == 0x5780

#include "bau5780.h"
#include "bits.h"
#include <string.h>
#include <stdio.h>

using namespace std;

/* ---- _Bau5780DualDLL ---------------------------------------------------- */

_Bau5780DualDLL::_Bau5780DualDLL(DeviceObject& devObj, NetworkLayerEntity& ne,
                                  Platform& plat,
                                  _Bau5780IpDLL& ip, _Bau5780TpDLL& tp)
    : DataLinkLayer(devObj, ne, plat), _ip(ip), _tp(tp)
{}

bool _Bau5780DualDLL::enabled() const { return _ip.enabled(); }

void _Bau5780DualDLL::enabled(bool v) { _ip.enabled(v); _tp.enabled(v); }

bool _Bau5780DualDLL::sendFrame(CemiFrame& frame)
{
    bool ok = _ip.sendFrameEx(frame);
    _tp.sendFrameEx(frame);
    return ok;
}

/* ---- Bau5780 ------------------------------------------------------------- */

Bau5780::Bau5780(Platform& platform)
    : BauSystemBDevice(platform), DataLinkLayerCallbacks(),
      _ipParameters(_deviceObj, platform),
      _dlLayerIp(_deviceObj, _ipParameters, _netLayer.getInterface(), platform,
                 (DataLinkLayerCallbacks*) this),
      _dlLayerTp(_deviceObj, _netLayer.getInterface(), platform,
                 (ITpUartCallBacks&) *this, (DataLinkLayerCallbacks*) this),
      _sendDLL(_deviceObj, _netLayer.getInterface(), platform, _dlLayerIp, _dlLayerTp)
#ifdef USE_CEMI_SERVER
    , _cemiServer(*this)
#endif
{
    /* Both sub-DLLs deliver received frames to the same NetworkLayerEntity.
     * _sendDLL is registered as the outgoing DLL and forwards to both IP+TP. */
    _netLayer.getInterface().dataLinkLayer(_sendDLL);

#ifdef USE_CEMI_SERVER
    _cemiServerObject.setMediumTypeAsSupported(DptMedium::KNX_IP);
    _cemiServerObject.setMediumTypeAsSupported(DptMedium::KNX_TP1);
    _cemiServer.dataLinkLayer(_dlLayerIp);
    _cemiServer.dataLinkLayerPrimary(_dlLayerIp);
    _dlLayerIp.cemiServer(_cemiServer);
    _memory.addSaveRestore(&_cemiServerObject);
#endif

    _memory.addSaveRestore(&_ipParameters);

    _deviceObj.maskVersion(0x5780);

    Property* prop = _deviceObj.property(PID_IO_LIST);
    prop->write(1, (uint16_t) OT_DEVICE);
    prop->write(2, (uint16_t) OT_ADDR_TABLE);
    prop->write(3, (uint16_t) OT_ASSOC_TABLE);
    prop->write(4, (uint16_t) OT_GRP_OBJ_TABLE);
    prop->write(5, (uint16_t) OT_APPLICATION_PROG);
    prop->write(6, (uint16_t) OT_IP_PARAMETER);
#if defined(USE_DATASECURE) && defined(USE_CEMI_SERVER)
    prop->write(7, (uint16_t) OT_SECURITY);
    prop->write(8, (uint16_t) OT_CEMI_SERVER);
#elif defined(USE_DATASECURE)
    prop->write(7, (uint16_t) OT_SECURITY);
#elif defined(USE_CEMI_SERVER)
    prop->write(7, (uint16_t) OT_CEMI_SERVER);
#endif
}

InterfaceObject* Bau5780::getInterfaceObject(uint8_t idx)
{
    switch (idx)
    {
        case 0: return &_deviceObj;
        case 1: return &_addrTable;
        case 2: return &_assocTable;
        case 3: return &_groupObjTable;
        case 4: return &_appProgram;
        case 5: return nullptr;
        case 6: return &_ipParameters;
#if defined(USE_DATASECURE) && defined(USE_CEMI_SERVER)
        case 7: return &_secIfObj;
        case 8: return &_cemiServerObject;
#elif defined(USE_CEMI_SERVER)
        case 7: return &_cemiServerObject;
#elif defined(USE_DATASECURE)
        case 7: return &_secIfObj;
#endif
        default: return nullptr;
    }
}

InterfaceObject* Bau5780::getInterfaceObject(ObjectType objectType, uint16_t objectInstance)
{
    (void) objectInstance;
    switch (objectType)
    {
        case OT_DEVICE:            return &_deviceObj;
        case OT_ADDR_TABLE:        return &_addrTable;
        case OT_ASSOC_TABLE:       return &_assocTable;
        case OT_GRP_OBJ_TABLE:     return &_groupObjTable;
        case OT_APPLICATION_PROG:  return &_appProgram;
        case OT_IP_PARAMETER:      return &_ipParameters;
#ifdef USE_DATASECURE
        case OT_SECURITY:          return &_secIfObj;
#endif
#ifdef USE_CEMI_SERVER
        case OT_CEMI_SERVER:       return &_cemiServerObject;
#endif
        default:                   return nullptr;
    }
}

void Bau5780::doMasterReset(EraseCode eraseCode, uint8_t channel)
{
    BauSystemB::doMasterReset(eraseCode, channel);
    _ipParameters.masterReset(eraseCode, channel);
}

bool Bau5780::enabled()       { return _dlLayerIp.enabled() && _dlLayerTp.enabled(); }
void Bau5780::enabled(bool v) { _dlLayerIp.enabled(v); _dlLayerTp.enabled(v); }

void Bau5780::syncCemiClientAddress()
{
#ifdef USE_CEMI_SERVER
    _cemiServer.clientAddress(_deviceObj.individualAddress() + 1u);
#endif
}

void Bau5780::loop()
{
    _dlLayerIp.loop();
    _dlLayerTp.loop();
    BauSystemBDevice::loop();
#ifdef USE_CEMI_SERVER
    _cemiServer.loop();
#endif
}

TPAckType Bau5780::isAckRequired(uint16_t address, bool isGrpAddr)
{
    if (isGrpAddr)
    {
        if (address == 0)
            return TPAckType::AckReqAck;
        if (_addrTable.contains(address))
            return TPAckType::AckReqAck;
        return TPAckType::AckReqNone;
    }
    if (address == _deviceObj.individualAddress())
        return TPAckType::AckReqAck;
    return TPAckType::AckReqNone;
}

_Bau5780IpDLL* Bau5780::getIpDataLinkLayer() { return &_dlLayerIp; }
_Bau5780TpDLL* Bau5780::getTpDataLinkLayer() { return &_dlLayerTp; }

#endif /* MASK_VERSION == 0x5780 */
