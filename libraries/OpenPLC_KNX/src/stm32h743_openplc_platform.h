#pragma once

/*
 * stm32h743_openplc_platform.h
 *
 * Concrete Platform implementation for the OpenPLC Bridge board (STM32H743).
 * Inherits directly from Platform (not ArduinoPlatform) - pure HAL + LwIP.
 *
 * Responsibilities:
 *   IP     - LwIP UDP multicast (KNXnet/IP routing)
 *   NVM    - HAL Flash erase/program (Bank 2 Sector 6, reference library data)
 *   System - HAL_GetUID, NVIC_SystemReset, netif_default
 */

#include <stdint.h>
#include <stddef.h>
#include <stm32h7xx_hal.h>

extern "C" {
#include "lwip/udp.h"
#include "lwip/pbuf.h"
#include "lwip/ip_addr.h"
#include "lwip/igmp.h"
#include "lwip/netif.h"
}

/* MASK_VERSION comes from the sketch's OPENPLC_KNX_ROLE() line, through the
 * header prebuild.sh generates and force-includes into every unit.
 * $PROD/docs/modules/M3-app-runtime.md R3-24 */
#ifndef MASK_VERSION
#  error "OpenPLC_KNX: no KNX role. Add OPENPLC_KNX_ROLE(TP_DEVICE); or OPENPLC_KNX_ROLE(IP_ROUTER); on its own line in the .ino file"
#endif
#ifndef KNX_NO_AUTOMATIC_GLOBAL_INSTANCE
#  define KNX_NO_AUTOMATIC_GLOBAL_INSTANCE
#endif

#include <knx/platform.h>
#include "knx_config.h"

/* -------------------------------------------------------------------------
 * Platform class
 * ---------------------------------------------------------------------- */

class Stm32H743OpenPLCPlatform : public Platform
{
public:
    Stm32H743OpenPLCPlatform();
    ~Stm32H743OpenPLCPlatform() override;

    /* --- System -------------------------------------------------------- */
    void     restart()            override;
    void     restartRequested(bool masterReset, uint8_t eraseCode) override;
    void     fatalError()         override;
    uint32_t uniqueSerialNumber() override;

    /* --- IP configuration (read from LwIP netif_default) --------------- */
    uint32_t currentIpAddress()      override;
    uint32_t currentSubnetMask()     override;
    uint32_t currentDefaultGateway() override;
    void     macAddress(uint8_t* data) override;

    /* --- IP multicast (LwIP UDP) --------------------------------------- */
    void setupMultiCast(uint32_t addr, uint16_t port)              override;
    void closeMultiCast()                                          override;
    bool sendBytesMultiCast(uint8_t* buffer, uint16_t len)         override;
    int  readBytesMultiCast(uint8_t* buffer, uint16_t maxLen)      override;
    int  readBytesMultiCast(uint8_t* buffer, uint16_t maxLen,
                            uint32_t& srcAddr, uint16_t& srcPort)  override;
    bool sendBytesUniCast(uint32_t addr, uint16_t port,
                          uint8_t* buffer, uint16_t len)           override;

    /* --- NVM (Eeprom-mode, backed by HAL Flash Bank 2 Sector 6) ------- */
    uint8_t* getEepromBuffer(uint32_t size) override;
    void     commitToEeprom()               override;

private:
    /* LwIP UDP multicast */
    struct udp_pcb *_udpPcb;
    ip4_addr_t      _mcastAddr;
    uint16_t        _mcastPort;
    uint32_t        _lastSrcAddr;
    uint16_t        _lastSrcPort;
    uint8_t         _ipRxBuf[KNX_IP_RXBUF_SIZE];
    uint16_t        _ipRxLen;

    /* NVM (Eeprom type) */
    uint8_t *_eepromBuf;
    uint32_t _eepromSize;

    /* LwIP receive callback (static → routes to instance) */
    static void _udpRecvCb(void* arg, struct udp_pcb* pcb, struct pbuf* p,
                           const ip_addr_t* addr, u16_t port);
    void _onUdpReceive(struct pbuf* p, const ip_addr_t* addr, u16_t port);
};
