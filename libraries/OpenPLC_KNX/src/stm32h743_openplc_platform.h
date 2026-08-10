#pragma once

/*
 * stm32h743_openplc_platform.h
 *
 * Concrete Platform implementation for the OpenPLC Bridge board (STM32H743).
 * Inherits directly from Platform (not ArduinoPlatform) - pure HAL + LwIP.
 *
 * Responsibilities:
 *   UART   - USART1 interrupt-driven RX ring-buffer (STKNX TP transceiver)
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

/* Must be defined before pulling in any reference-library header */
#ifndef MASK_VERSION
#  define MASK_VERSION 0x5780u   /* IP+TP dual device by default; no menu selection needed */
#endif
#ifndef KNX_NO_AUTOMATIC_GLOBAL_INSTANCE
#  define KNX_NO_AUTOMATIC_GLOBAL_INSTANCE
#endif

/* Suppress the reference library's global print/println free-function
 * declarations.  On ARDUINO_ARCH_STM32 the implementations are never
 * provided (bits.cpp only implements them for ESP_PLATFORM), which causes
 * linker errors.  The KNX stack debug output is not needed in production. */
#ifndef KNX_NO_PRINT
#  define KNX_NO_PRINT
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
    void     fatalError()         override;
    uint32_t uniqueSerialNumber() override;

    /* --- IP configuration (read from LwIP netif_default) --------------- */
    uint32_t currentIpAddress()      override;
    uint32_t currentSubnetMask()     override;
    uint32_t currentDefaultGateway() override;
    void     macAddress(uint8_t* data) override;

    /* --- UART (STKNX via USART1, interrupt-driven) --------------------- */
    void   setupUart()                                     override;
    void   closeUart()                                     override;
    int    uartAvailable()                                 override;
    size_t writeUart(const uint8_t data)                   override;
    size_t writeUart(const uint8_t* buffer, size_t size)   override;
    int    readUart()                                      override;
    size_t readBytesUart(uint8_t* buffer, size_t length)   override;
    bool   overflowUart()                                  override;
    void   flushUart()                                     override;

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

    /* Called from USART1_IRQHandler - do not call from application code */
    void _uartRxByteISR(uint8_t byte);

private:
    /* UART */
    UART_HandleTypeDef  _huart;
    uint8_t             _uartRxBuf[KNX_UART_RXBUF_SIZE];
    volatile uint16_t   _uartRxHead;  /* write index (ISR) */
    volatile uint16_t   _uartRxTail;  /* read index (main) */
    volatile bool       _uartOverflow;

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

    /* HAL Flash helpers */
    bool _flashEraseSector(uint32_t bank, uint32_t sector);
    bool _flashWriteBuffer(uint32_t addr, const uint8_t* data, size_t size);

    /* LwIP receive callback (static → routes to instance) */
    static void _udpRecvCb(void* arg, struct udp_pcb* pcb, struct pbuf* p,
                           const ip_addr_t* addr, u16_t port);
    void _onUdpReceive(struct pbuf* p, const ip_addr_t* addr, u16_t port);
};
