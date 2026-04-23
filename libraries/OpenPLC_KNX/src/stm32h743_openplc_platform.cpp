#include "stm32h743_openplc_platform.h"
#include "uart.h"       /* STM32duino low-level UART API (uart_init, etc.) */
#include "PinNames.h"   /* PA_10_ALT1, PB_14 */
#include <string.h>
#include <stdlib.h>

/* =========================================================================
 * USART1 plumbing via the STM32duino serial_t API
 *
 * Using uart_init() / uart_attach_rx_callback() registers our handle in the
 * SrcWrapper's uart_handlers[] table, so SrcWrapper's USART1_IRQHandler
 * (uart.c) correctly calls HAL_UART_IRQHandler on our handle — no custom
 * ISR needed here.
 *
 * TX: blocking HAL_UART_Transmit (acceptable at 19200 bps).
 * RX: interrupt-driven via HAL_UART_Receive_IT (1 byte at a time); the
 *     HAL_UART_RxCpltCallback in uart.c dispatches to _serialRxCb().
 * ======================================================================= */

static Stm32H743OpenPLCPlatform *s_platform = nullptr;
static serial_t                   s_serial;   /* registered with SrcWrapper */

static void _serialRxCb(serial_t *obj)
{
    if (s_platform != nullptr) {
        s_platform->_uartRxByteISR(obj->recv);
    }
    /* Re-arm for the next byte (mirrors the uart_getc() pattern) */
    HAL_UART_Receive_IT(&obj->handle, &obj->recv, 1u);
}

/* USART1_IRQHandler is provided by SrcWrapper (uart.c).
 * It calls HAL_UART_IRQHandler(uart_handlers[UART1_INDEX]) which dispatches
 * to HAL_UART_RxCpltCallback → _serialRxCb() above. */

/* =========================================================================
 * Constructor / destructor
 * ======================================================================= */

Stm32H743OpenPLCPlatform::Stm32H743OpenPLCPlatform()
    : _uartRxHead(0u), _uartRxTail(0u), _uartOverflow(false)
    , _udpPcb(nullptr), _mcastPort(0u)
    , _lastSrcAddr(0u), _lastSrcPort(0u)
    , _ipRxLen(0u)
    , _eepromBuf(nullptr), _eepromSize(0u)
{
    memset(&s_serial,   0, sizeof(s_serial));
    memset(&_mcastAddr, 0, sizeof(_mcastAddr));
    memset(_uartRxBuf,  0, sizeof(_uartRxBuf));
    memset(_ipRxBuf,    0, sizeof(_ipRxBuf));
    s_platform  = this;
}

Stm32H743OpenPLCPlatform::~Stm32H743OpenPLCPlatform()
{
    closeUart();
    closeMultiCast();
    free(_eepromBuf);
    s_platform = nullptr;
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
 * Network info — read from LwIP netif_default
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
 * UART — USART1, 19200 8E1, interrupt-driven RX ring buffer
 * ======================================================================= */

void Stm32H743OpenPLCPlatform::setupUart()
{
    /* Use the STM32duino serial_t API so uart_handlers[UART1_INDEX] is
     * populated and SrcWrapper's USART1_IRQHandler works correctly.
     * uart_init() handles clocks, GPIO alternate-function config, HAL init,
     * NVIC enable, and registration in the SrcWrapper handler table. */
    s_serial.pin_tx = PB_14;       /* USART1 TX: PB14 / AF4 */
    s_serial.pin_rx = PA_10_ALT1;  /* USART1 RX: PA10 / AF7 (ALT1 = USART1) */
    s_serial.pin_rts = NC;
    s_serial.pin_cts = NC;

    /* KNX TP is 8E1: 8 data bits + 1 even-parity bit.  On STM32, enabling
     * parity steals one bit from the frame, so the word-length field must
     * count the parity bit too → WORDLENGTH_9B = 8 data + 1 parity = 9 total. */
    uart_init(&s_serial,
              KNX_USART_BAUD,
              UART_WORDLENGTH_9B,
              UART_PARITY_EVEN,
              UART_STOPBITS_1);

    /* Override the default UART_IRQ_PRIO (1) with our configured priority.
     * Must be done after uart_init() which sets the NVIC priority. */
    HAL_NVIC_SetPriority(KNX_USART_IRQn, KNX_UART_IRQ_PRIORITY, 0u);

    /* Arm interrupt-driven RX (1 byte at a time) via SrcWrapper callback */
    uart_attach_rx_callback(&s_serial, _serialRxCb);
}

void Stm32H743OpenPLCPlatform::closeUart()
{
    HAL_NVIC_DisableIRQ(KNX_USART_IRQn);
    HAL_UART_DeInit(&s_serial.handle);
    _uartRxHead = 0u;
    _uartRxTail = 0u;
    _uartOverflow = false;
}

void Stm32H743OpenPLCPlatform::_uartRxByteISR(uint8_t byte)
{
    uint16_t next = (_uartRxHead + 1u) & (KNX_UART_RXBUF_SIZE - 1u);
    if (next == _uartRxTail) {
        /* Buffer full: drop the incoming byte (drop-newest policy).
         * KNX TP retransmission handles the lost frame at protocol level. */
        _uartOverflow = true;
        return;
    }
    _uartRxBuf[_uartRxHead] = byte;
    _uartRxHead = next;
}

int Stm32H743OpenPLCPlatform::uartAvailable()
{
    return (int)((_uartRxHead - _uartRxTail) & (KNX_UART_RXBUF_SIZE - 1u));
}

int Stm32H743OpenPLCPlatform::readUart()
{
    if (_uartRxHead == _uartRxTail) return -1;
    uint8_t byte = _uartRxBuf[_uartRxTail];
    _uartRxTail  = (_uartRxTail + 1u) & (KNX_UART_RXBUF_SIZE - 1u);
    return (int)byte;
}

size_t Stm32H743OpenPLCPlatform::readBytesUart(uint8_t* buffer, size_t length)
{
    size_t n = 0u;
    while (n < length) {
        int b = readUart();
        if (b < 0) break;
        buffer[n++] = (uint8_t)b;
    }
    return n;
}

size_t Stm32H743OpenPLCPlatform::writeUart(const uint8_t data)
{
    HAL_UART_Transmit(&s_serial.handle, const_cast<uint8_t*>(&data), 1u, 10u);
    return 1u;
}

size_t Stm32H743OpenPLCPlatform::writeUart(const uint8_t* buffer, size_t size)
{
    HAL_UART_Transmit(&s_serial.handle, const_cast<uint8_t*>(buffer),
                      (uint16_t)size, (uint32_t)(size * 2u + 5u));
    return size;
}

bool Stm32H743OpenPLCPlatform::overflowUart()
{
    bool v = _uartOverflow;
    _uartOverflow = false;
    return v;
}

void Stm32H743OpenPLCPlatform::flushUart()
{
    _uartRxHead = 0u;
    _uartRxTail = 0u;
    _uartOverflow = false;
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
        /* Previous packet not yet consumed — drop this one */
        pbuf_free(p);
        return;
    }
    if (p->tot_len > KNX_IP_RXBUF_SIZE) {
        /* oversized packet — truncated to KNX_IP_RXBUF_SIZE */
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
 * NVM — Eeprom type, backed by Flash Bank 2 Sector 6
 *
 * getEepromBuffer: allocate RAM buffer, load from Flash on first call
 * commitToEeprom:  erase sector, write buffer back in 32-byte chunks
 * ======================================================================= */

bool Stm32H743OpenPLCPlatform::_flashEraseSector(uint32_t bank, uint32_t sector)
{
    FLASH_EraseInitTypeDef eraseInit;
    eraseInit.TypeErase = FLASH_TYPEERASE_SECTORS;
    eraseInit.Banks     = bank;
    eraseInit.Sector    = sector;
    eraseInit.NbSectors = 1u;

    uint32_t sectorError = 0u;
    return (HAL_FLASHEx_Erase(&eraseInit, &sectorError) == HAL_OK);
}

bool Stm32H743OpenPLCPlatform::_flashWriteBuffer(uint32_t       addr,
                                                   const uint8_t* data,
                                                   size_t         size)
{
    uint8_t buf[32u] __attribute__((aligned(32u)));
    size_t  offset = 0u;

    while (offset < size) {
        memset(buf, 0xFFu, 32u);
        size_t chunk = ((size - offset) > 32u) ? 32u : (size - offset);
        memcpy(buf, data + offset, chunk);

        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD,
                              addr + offset,
                              (uint32_t)(uintptr_t)buf) != HAL_OK) {
            return false;
        }
        /* Round up to next 32-byte boundary */
        offset += 32u;
    }
    return true;
}

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

    HAL_FLASH_Unlock();

    bool ok = _flashEraseSector(KNX_STACK_NVM_FLASH_BANK,
                                 KNX_STACK_NVM_FLASH_SECTOR);
    if (ok) {
        ok = _flashWriteBuffer(KNX_STACK_NVM_FLASH_ADDR,
                                _eepromBuf, _eepromSize);
    }

    HAL_FLASH_Lock();
    (void)ok; /* caller (Platform base) doesn't check return value */
}
