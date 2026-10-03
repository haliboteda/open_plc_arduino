#include "stm32h743_openplc_platform.h"
#include "knx_nvm.h"
#include <string.h>
#include <stdlib.h>

/* =========================================================================
 * Constructor / destructor
 * ======================================================================= */

Stm32H743OpenPLCPlatform::Stm32H743OpenPLCPlatform()
    : _udpPcb(nullptr), _mcastPort(0u)
    , _lastSrcAddr(0u), _lastSrcPort(0u)
    , _ipRxLen(0u)
    , _eepromBuf(nullptr), _eepromSize(0u)
{
    memset(&_mcastAddr, 0, sizeof(_mcastAddr));
    memset(_ipRxBuf,    0, sizeof(_ipRxBuf));
}

Stm32H743OpenPLCPlatform::~Stm32H743OpenPLCPlatform()
{
    closeMultiCast();
    free(_eepromBuf);
}

/* =========================================================================
 * System
 * ======================================================================= */

void Stm32H743OpenPLCPlatform::restart()
{
    NVIC_SystemReset();
}

void Stm32H743OpenPLCPlatform::fatalError()
{
    __disable_irq();
    while (true) { /* halt */ }
}

uint32_t Stm32H743OpenPLCPlatform::uniqueSerialNumber()
{
    return HAL_GetUIDw0() ^ HAL_GetUIDw1() ^ HAL_GetUIDw2();
}

/* =========================================================================
 * Network info - read from LwIP netif_default
 *
 * LwIP stores addresses in network byte order (big-endian).
 * The reference library expects host byte order, so lwip_ntohl() is applied
 * to every address returned here.
 * ======================================================================= */

uint32_t Stm32H743OpenPLCPlatform::currentIpAddress()
{
    if (netif_default != nullptr)
        return lwip_ntohl(netif_ip4_addr(netif_default)->addr);
    return 0u;
}

uint32_t Stm32H743OpenPLCPlatform::currentSubnetMask()
{
    if (netif_default != nullptr)
        return lwip_ntohl(netif_ip4_netmask(netif_default)->addr);
    return 0u;
}

uint32_t Stm32H743OpenPLCPlatform::currentDefaultGateway()
{
    if (netif_default != nullptr)
        return lwip_ntohl(netif_ip4_gw(netif_default)->addr);
    return 0u;
}

void Stm32H743OpenPLCPlatform::macAddress(uint8_t* data)
{
    if (netif_default != nullptr && data != nullptr)
        memcpy(data, netif_default->hwaddr, 6u);
}

/* =========================================================================
 * LwIP UDP multicast
 *
 * addr is passed in host byte order (e.g. 0xE000170C for 224.0.23.12)
 * by the reference library.  LwIP requires network byte order internally.
 * ======================================================================= */

void Stm32H743OpenPLCPlatform::_udpRecvCb(void* arg, struct udp_pcb* /*pcb*/,
                                           struct pbuf* p,
                                           const ip_addr_t* addr, u16_t port)
{
    auto *self = static_cast<Stm32H743OpenPLCPlatform*>(arg);
    if (self != nullptr && p != nullptr) {
        self->_onUdpReceive(p, addr, port);
    } else {
        if (p) pbuf_free(p);
    }
}

void Stm32H743OpenPLCPlatform::_onUdpReceive(struct pbuf* p,
                                              const ip_addr_t* addr,
                                              u16_t port)
{
    if (_ipRxLen > 0u) {
        /* Previous packet not yet consumed - drop this one */
        pbuf_free(p);
        return;
    }
    if (p->tot_len > KNX_IP_RXBUF_SIZE) {
        /* oversized packet - truncated to KNX_IP_RXBUF_SIZE */
    }
    uint16_t len = (p->tot_len < KNX_IP_RXBUF_SIZE)
                   ? (uint16_t)p->tot_len : (uint16_t)KNX_IP_RXBUF_SIZE;
    pbuf_copy_partial(p, _ipRxBuf, len, 0u);
    _ipRxLen     = len;
    _lastSrcAddr = addr ? lwip_ntohl(ip4_addr_get_u32(ip_2_ip4(addr))) : 0u;
    _lastSrcPort = port;
    pbuf_free(p);
}

void Stm32H743OpenPLCPlatform::setupMultiCast(uint32_t addr, uint16_t port)
{
    closeMultiCast();

    /* Convert host-order addr to LwIP network-order ip4_addr_t */
    _mcastAddr.addr = lwip_htonl(addr);
    _mcastPort      = port;
    _ipRxLen        = 0u;

    _udpPcb = udp_new();
    if (_udpPcb == nullptr) return;

    ip_set_option(_udpPcb, SOF_REUSEADDR);
    udp_bind(_udpPcb, IP4_ADDR_ANY, port);

#if LWIP_IGMP
    igmp_joingroup(IP4_ADDR_ANY, &_mcastAddr);
#endif

    udp_recv(_udpPcb, _udpRecvCb, this);
}

void Stm32H743OpenPLCPlatform::closeMultiCast()
{
    if (_udpPcb == nullptr) return;

#if LWIP_IGMP
    igmp_leavegroup(IP4_ADDR_ANY, &_mcastAddr);
#endif

    udp_remove(_udpPcb);
    _udpPcb  = nullptr;
    _ipRxLen = 0u;
}

bool Stm32H743OpenPLCPlatform::sendBytesMultiCast(uint8_t* buffer, uint16_t len)
{
    if (_udpPcb == nullptr) return false;

    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, len, PBUF_RAM);
    if (p == nullptr) return false;

    memcpy(p->payload, buffer, len);

    ip_addr_t dst;
    ip4_addr_copy(*ip_2_ip4(&dst), _mcastAddr);
    IP_SET_TYPE_VAL(dst, IPADDR_TYPE_V4);

    err_t err = udp_sendto(_udpPcb, p, &dst, _mcastPort);
    pbuf_free(p);
    return (err == ERR_OK);
}

int Stm32H743OpenPLCPlatform::readBytesMultiCast(uint8_t* buffer,
                                                  uint16_t maxLen)
{
    if (_ipRxLen == 0u) return 0;
    uint16_t n = (_ipRxLen < maxLen) ? _ipRxLen : maxLen;
    memcpy(buffer, _ipRxBuf, n);
    _ipRxLen = 0u;
    return (int)n;
}

int Stm32H743OpenPLCPlatform::readBytesMultiCast(uint8_t* buffer,
                                                  uint16_t maxLen,
                                                  uint32_t& srcAddr,
                                                  uint16_t& srcPort)
{
    int n = readBytesMultiCast(buffer, maxLen);
    if (n > 0) {
        srcAddr = _lastSrcAddr;
        srcPort = _lastSrcPort;
    }
    return n;
}

bool Stm32H743OpenPLCPlatform::sendBytesUniCast(uint32_t addr, uint16_t port,
                                                 uint8_t* buffer, uint16_t len)
{
    if (_udpPcb == nullptr) return false;

    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, len, PBUF_RAM);
    if (p == nullptr) return false;

    memcpy(p->payload, buffer, len);

    ip_addr_t dst;
    ip4_addr_set_u32(ip_2_ip4(&dst), lwip_htonl(addr));
    IP_SET_TYPE_VAL(dst, IPADDR_TYPE_V4);

    err_t err = udp_sendto(_udpPcb, p, &dst, port);
    pbuf_free(p);
    return (err == ERR_OK);
}

/* =========================================================================
 * NVM - Eeprom type, backed by the KNX sector (layout in knx_config.h)
 *
 * getEepromBuffer: allocate RAM buffer, load from Flash on first call
 * commitToEeprom:  rewrite the sector, keeping the application NVM block
 * ======================================================================= */

uint8_t* Stm32H743OpenPLCPlatform::getEepromBuffer(uint32_t size)
{
    if (_eepromBuf == nullptr) {
        _eepromBuf  = static_cast<uint8_t*>(malloc(size));
        _eepromSize = size;
        if (_eepromBuf != nullptr) {
            /* Load existing data from Flash (memory-mapped read) */
            memcpy(_eepromBuf,
                   reinterpret_cast<const void*>(KNX_STACK_NVM_FLASH_ADDR),
                   size);
        }
    }
    return _eepromBuf;
}

void Stm32H743OpenPLCPlatform::commitToEeprom()
{
    if (_eepromBuf == nullptr || _eepromSize == 0u) return;

    /* caller (Platform base) doesn't check the return value */
    (void)knx_nvm_sector_write(_eepromBuf, _eepromSize, NULL);
}
