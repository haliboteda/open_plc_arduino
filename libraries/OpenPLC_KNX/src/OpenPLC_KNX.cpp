#include "OpenPLC_KNX.h"
#include "knx_config.h"
#include "stknx_phy.h"
#include "stknx_data_link_layer.h"
#include "stm32/interrupt.h"
#include <string.h>

/* =========================================================================
 * The stack instance, mask selected by MASK_VERSION (the sketch's OPENPLC_KNX_ROLE()).
 * Internal, not part of the sketch API: sketches use KNX (decision 109).
 * KNX_NO_AUTOMATIC_GLOBAL_INSTANCE keeps knx_facade.cpp from making another.
 * ======================================================================= */
#if MASK_VERSION == 0x07B0
static KnxFacade<Stm32H743OpenPLCPlatform, Bau07B0> knxStack;
#else
static KnxFacade<Stm32H743OpenPLCPlatform, Bau091A> knxStack;
#endif

OpenPLC_KNX_Class KNX;

static KnxBusSendDoneCallback s_sendDone;

/* -------------------------------------------------------------------------
 * Prog-button EXTI interrupt service routine: rising edge of PG9 (the button
 * pulls it high), 200 ms debounce.
 * ---------------------------------------------------------------------- */
static void progButtonISR()
{
    static uint32_t lastToggleMs = 0u;
    uint32_t now = HAL_GetTick();
    if ((now - lastToggleMs) > 200u) {
        knxStack.toggleProgMode();
        lastToggleMs = now;
    }
}

/* -------------------------------------------------------------------------
 * Prog-LED callbacks - use HAL GPIO (not Arduino digitalWrite). The line goes
 * to the system log (R3-22) so a person, or a test script, sees the mode change.
 * ---------------------------------------------------------------------- */
static void logLine(const char *text)
{
    openplc_syslog_write(OPENPLC_SYSLOG_TAG_KNX, text, strlen(text));
}

static void progLedOn()
{
    logLine("programming mode on");
    HAL_GPIO_WritePin(KNX_PROG_LED_PORT, KNX_PROG_LED_PIN, GPIO_PIN_SET);
}

static void progLedOff()
{
    logLine("programming mode off");
    HAL_GPIO_WritePin(KNX_PROG_LED_PORT, KNX_PROG_LED_PIN, GPIO_PIN_RESET);
}

/* -------------------------------------------------------------------------
 * setup()
 * ---------------------------------------------------------------------- */
void OpenPLC_KNX_Class::setup(const char* serial, uint16_t mfr_id)
{
    /* First, whatever the role: a floating KNX_TX makes the transceiver draw
     * current from the bus. */
    stknx_phy_park();

    /* Enable GPIO clocks before any HAL_GPIO_Init calls */
    __HAL_RCC_GPIOG_CLK_ENABLE();   /* PG9  = prog-button                 */
    __HAL_RCC_GPIOE_CLK_ENABLE();   /* PE2  = system LED, prog-mode       */

    /* Configure prog-LED GPIO as output */
    {
        GPIO_InitTypeDef init = {0};
        init.Pin   = KNX_PROG_LED_PIN;
        init.Mode  = GPIO_MODE_OUTPUT_PP;
        init.Pull  = GPIO_NOPULL;
        init.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(KNX_PROG_LED_PORT, &init);
    }
    progLedOff();

    /* Configure the prog-button pull first (stm32_interrupt_enable reads
     * the current PUPDR register to preserve it during GPIO re-init). None:
     * the board pulls PG9 down and the button pulls it up - an internal
     * pull-up would fight the 10k pull-down. */
    {
        GPIO_InitTypeDef init = {0};
        init.Pin   = KNX_PROG_KEY_PIN;
        init.Mode  = GPIO_MODE_INPUT;
        init.Pull  = GPIO_NOPULL;
        init.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(KNX_PROG_KEY_PORT, &init);
    }
    /* Register callback through STM32duino EXTI infrastructure so we do
     * not conflict with SrcWrapper's EXTI9_5_IRQHandler definition. */
    stm32_interrupt_enable(KNX_PROG_KEY_PORT, KNX_PROG_KEY_PIN,
                           progButtonISR,
                           GPIO_MODE_IT_RISING);

    /* Configure the stack: use callbacks for LED, bypass Arduino pin logic */
    knxStack.ledPin(-1);                   /* disable Arduino LED control */
    knxStack.buttonPin(-1);                /* disable Arduino button control */
    knxStack.setProgLedOnCallback(progLedOn);
    knxStack.setProgLedOffCallback(progLedOff);

    /* Device identity */
    knxStack.manufacturerId(mfr_id);
    knxStack.bauNumber(knxStack.platform().uniqueSerialNumber());

    /* Order / hardware type from serial string (padded to 10 bytes) */
    if (serial != nullptr) {
        uint8_t order[10u] = {0};
        size_t  len = strlen(serial);
        if (len > 10u) len = 10u;
        memcpy(order, serial, len);
        knxStack.orderNumber(order);
    }

    knxStack.version(0x0100u);

    /* Role (mask version) in hardwareType, so readMemory() discards data
     * another role wrote instead of restoring it into the wrong layout.
     * See $PROD/docs/modules/M3-app-runtime.md, R3-20. */
    {
        uint8_t hw[LEN_HARDWARE_TYPE] = {0};
        hw[0] = (uint8_t)((MASK_VERSION >> 8) & 0xFFu);
        hw[1] = (uint8_t)(MASK_VERSION & 0xFFu);
        knxStack.hardwareType(hw);
    }

    knxStack.individualAddress(KNX_DEFAULT_INDIVIDUAL_ADDR);

    /* Load KNX stack NVM (ETS-programmed address table etc.)
     * This may restore a different individual address programmed by ETS,
     * so syncCemiClientAddress() must be called AFTER readMemory(). */
    knxStack.readMemory();

    /* Now that the final IA is known, synchronise the cEMI client address
     * (clientAddress = IA + 1 used by ETS tunnelling connection). */
    knxStack.bau().syncCemiClientAddress();
}

/* -------------------------------------------------------------------------
 * start()
 * ---------------------------------------------------------------------- */
void OpenPLC_KNX_Class::start()
{
    knxStack.start();
}

/* -------------------------------------------------------------------------
 * loop()
 * ---------------------------------------------------------------------- */
void OpenPLC_KNX_Class::loop()
{
    knxStack.loop();
}

/* -------------------------------------------------------------------------
 * Status and group objects
 * ---------------------------------------------------------------------- */
bool OpenPLC_KNX_Class::progMode() const
{
    return knxStack.progMode();
}

bool OpenPLC_KNX_Class::configured() const
{
    return knxStack.configured();
}

uint16_t OpenPLC_KNX_Class::individualAddress() const
{
    return knxStack.individualAddress();
}

#if MASK_VERSION == 0x07B0
GroupObject& OpenPLC_KNX_Class::getGroupObject(uint16_t n)
{
    return knxStack.getGroupObject(n);
}
#endif

/* -------------------------------------------------------------------------
 * TP bus outside the stack. Frame layouts as in knx/tp_frame.h.
 * ---------------------------------------------------------------------- */
#ifdef USE_TP
static KnxBusFrameCallback s_busFrameCb;

static void busFrameThunk(const uint8_t* f, uint16_t n, bool forUs)
{
    KnxBusFrame b;
    uint8_t     hdr;

    if ((f[0] & 0xD3u) == 0x10u) {   /* extended frame */
        b.source      = (uint16_t)((f[2] << 8) | f[3]);
        b.destination = (uint16_t)((f[4] << 8) | f[5]);
        b.group       = (f[1] & 0x80u) != 0u;
        hdr           = 7u;
    } else {
        b.source      = (uint16_t)((f[1] << 8) | f[2]);
        b.destination = (uint16_t)((f[3] << 8) | f[4]);
        b.group       = (f[5] & 0x80u) != 0u;
        hdr           = 6u;
    }
    b.forUs   = forUs;
    b.tpdu    = &f[hdr];
    b.tpduLen = (uint8_t)(n - hdr - 1u);
    b.raw     = f;
    b.rawLen  = n;
    if (s_busFrameCb) {
        s_busFrameCb(b);
    }
}
#endif

void OpenPLC_KNX_Class::onBusFrame(KnxBusFrameCallback cb)
{
#ifdef USE_TP
    s_busFrameCb = cb;
    StknxDataLinkLayer::instance()->monitor(cb ? busFrameThunk : nullptr);
#else
    (void)cb;
#endif
}

static bool sendGroupFrame(uint16_t groupAddress, uint8_t apciLow,
                           const uint8_t* data, uint8_t len)
{
#ifdef USE_TP
    StknxDataLinkLayer* dl = StknxDataLinkLayer::instance();
    uint8_t  f[23];
    uint16_t ia = dl->individualAddress();
    uint8_t  n;

    f[0] = 0xBCu;                          /* standard frame, not repeated, low priority */
    f[1] = (uint8_t)(ia >> 8);
    f[2] = (uint8_t)ia;
    f[3] = (uint8_t)(groupAddress >> 8);
    f[4] = (uint8_t)groupAddress;
    f[5] = (uint8_t)(0x80u | 0x60u | (1u + len));   /* group, hop count 6, length */
    f[6] = 0x00u;                          /* T_Data_Group; APCI GroupValue.Write = 0x080 */
    f[7] = (uint8_t)(0x80u | apciLow);
    for (uint8_t i = 0; i < len; i++) {
        f[8 + i] = data[i];
    }
    n = (uint8_t)(8u + len);
    f[n] = 0xFFu;
    for (uint8_t i = 0; i < n; i++) {
        f[n] ^= f[i];                      /* check octet: inverted XOR */
    }
    return dl->sendRaw(f, (uint16_t)(n + 1u), s_sendDone);
#else
    (void)groupAddress; (void)apciLow; (void)data; (void)len;
    return false;
#endif
}

bool OpenPLC_KNX_Class::sendGroupWrite(uint16_t groupAddress, uint8_t smallValue)
{
    if (smallValue > 0x3Fu) {
        return false;
    }
    return sendGroupFrame(groupAddress, smallValue, nullptr, 0u);
}

bool OpenPLC_KNX_Class::sendGroupWrite(uint16_t groupAddress, const uint8_t* data, uint8_t len)
{
    if ((len == 0u) || (len > 14u)) {
        return false;
    }
    return sendGroupFrame(groupAddress, 0u, data, len);
}

void OpenPLC_KNX_Class::onBusSendDone(KnxBusSendDoneCallback cb)
{
    s_sendDone = cb;
}

/* EXTI9_5_IRQHandler is provided by SrcWrapper (interrupt.cpp).
 * The prog-button callback is registered via stm32_interrupt_enable()
 * in setup(), which routes through SrcWrapper's HAL_GPIO_EXTI_Callback
 * dispatch - no custom ISR needed here. */
