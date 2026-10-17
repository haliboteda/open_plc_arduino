#include "stknx_data_link_layer.h"
#ifdef USE_TP

#include "stknx_phy.h"
#include <stdlib.h>
#include <string.h>

StknxDataLinkLayer* StknxDataLinkLayer::_instance = nullptr;

StknxDataLinkLayer::StknxDataLinkLayer(DeviceObject& devObj, NetworkLayerEntity& netLayerEntity,
                                       Platform& platform, ITpUartCallBacks& cb,
                                       DataLinkLayerCallbacks* dllcb)
    : DataLinkLayer(devObj, netLayerEntity, platform), _cb(cb), _dllcb(dllcb)
{
    _instance = this;
}

void StknxDataLinkLayer::monitor(StknxBusFrameFn fn)
{
    _monitorFn = fn;
    _link.monitor = (fn != nullptr) ? 1u : 0u;
}

bool StknxDataLinkLayer::sendRaw(const uint8_t* frame, uint16_t len, StknxSendDoneFn done)
{
    if (!_enabled || _rawInFlight || (_rawLen != 0) || (len < 2u) || (len > STKNX_FRAME_MAX)) {
        return false;
    }
    memcpy(_raw, frame, len);
    _rawDone = done;
    _rawLen  = len;
    return true;
}

/* Interrupt context: the address tables are only read here. */
uint8_t StknxDataLinkLayer::addressed(void* ctx, uint16_t dst, uint8_t isGroup)
{
    StknxDataLinkLayer* self = static_cast<StknxDataLinkLayer*>(ctx);
    return self->_cb.isAckRequired(dst, isGroup != 0) != AckReqNone;
}

void StknxDataLinkLayer::confirm(TpFrame* frame, bool ok)
{
    uint8_t* cemiData = frame->cemiData();
    CemiFrame cemiFrame(cemiData, frame->cemiSize());
    dataConReceived(cemiFrame, ok);
    free(cemiData);
    delete frame;
}

void StknxDataLinkLayer::enabled(bool value)
{
    if (value == _enabled) {
        return;
    }
    if (value) {
        stknx_link_init(&_link, addressed, this);
        _link.monitor = (_monitorFn != nullptr) ? 1u : 0u;
        _enabled = stknx_phy_start(&_link);
        return;
    }
    stknx_phy_stop();
    _enabled = false;
    if (_txInFlight != nullptr) {
        confirm(_txInFlight, false);
        _txInFlight = nullptr;
    }
    while (_txCount != 0) {
        TpFrame* f = _txQueue[_txHead];
        _txHead = (uint8_t)((_txHead + 1u) % STKNX_TX_QUEUE);
        _txCount--;
        confirm(f, false);
    }
    if (_rawInFlight || (_rawLen != 0)) {
        _rawInFlight = false;
        _rawLen = 0;
        if (_rawDone) {
            _rawDone(false);
        }
    }
}

bool StknxDataLinkLayer::enabled() const
{
    return _enabled;
}

DptMedium StknxDataLinkLayer::mediumType() const
{
    return DptMedium::KNX_TP1;
}

bool StknxDataLinkLayer::sendFrame(CemiFrame& frame)
{
    if (!_enabled || (_txCount >= STKNX_TX_QUEUE)) {
        dataConReceived(frame, false);
        return false;
    }
    _txQueue[(_txHead + _txCount) % STKNX_TX_QUEUE] = new TpFrame(frame);
    _txCount++;
    return true;
}

void StknxDataLinkLayer::loop()
{
    static uint8_t buf[STKNX_FRAME_MAX];
    uint16_t n;
    uint8_t  forUs;

    if (!_enabled) {
        return;
    }

    while ((n = stknx_link_receive(&_link, buf, sizeof(buf), &forUs)) != 0u) {
        if (_monitorFn) {
            _monitorFn(buf, n, forUs != 0u);
        }
        if (!forUs) {
            continue;
        }
        TpFrame tpFrame(n);
        for (uint16_t i = 0; i < n; i++) {
            tpFrame.addByte(buf[i]);
        }
        uint8_t* cemiData = tpFrame.cemiData();
        CemiFrame cemiFrame(cemiData, tpFrame.cemiSize());
#ifdef KNX_ACTIVITYCALLBACK
        if (_dllcb)
            _dllcb->activity((_netIndex << KNX_ACTIVITYCALLBACK_NET) | (KNX_ACTIVITYCALLBACK_DIR_RECV << KNX_ACTIVITYCALLBACK_DIR));
#endif
        frameReceived(cemiFrame);
        free(cemiData);
    }

    /* One frame on the bus at a time, the stack's or a raw one. */
    if ((_txInFlight != nullptr) || _rawInFlight) {
        int r = stknx_link_send_result(&_link);
        if (r == STKNX_SEND_PENDING) {
            return;
        }
        if (_txInFlight != nullptr) {
            confirm(_txInFlight, r == STKNX_SEND_OK);
            _txInFlight = nullptr;
        } else {
            _rawInFlight = false;
            if (_rawDone) {
                _rawDone(r == STKNX_SEND_OK);
            }
        }
    }
    if (_rawLen != 0) {
        uint16_t len = _rawLen;
        _rawLen = 0;
        if (stknx_link_send(&_link, _raw, len)) {
            _rawInFlight = true;
        } else if (_rawDone) {
            _rawDone(false);
        }
    } else if (_txCount != 0) {
        _txInFlight = _txQueue[_txHead];
        _txHead = (uint8_t)((_txHead + 1u) % STKNX_TX_QUEUE);
        _txCount--;
        if (!stknx_link_send(&_link, _txInFlight->data(), _txInFlight->size())) {
            confirm(_txInFlight, false);
            _txInFlight = nullptr;
        }
#ifdef KNX_ACTIVITYCALLBACK
        else if (_dllcb)
            _dllcb->activity((_netIndex << KNX_ACTIVITYCALLBACK_NET) | (KNX_ACTIVITYCALLBACK_DIR_SEND << KNX_ACTIVITYCALLBACK_DIR));
#endif
    }
}

#endif
