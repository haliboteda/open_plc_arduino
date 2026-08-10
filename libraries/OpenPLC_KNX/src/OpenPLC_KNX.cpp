#include "OpenPLC_KNX.h"
#include "knx_config.h"
#include "stm32/interrupt.h"
#include "knx/table_object.h"
#include "knx/interface_object.h"
#include "knx/property.h"
#include <string.h>

/* =========================================================================
 * Global KNX instance (mask selected by MASK_VERSION, default Bau57B0)
 *
 * MASK_VERSION and KNX_NO_AUTOMATIC_GLOBAL_INSTANCE are defined in
 * stm32h743_openplc_platform.h (included transitively via OpenPLC_KNX.h),
 * so knx_facade.cpp will NOT create its own instance for ARDUINO_ARCH_STM32.
 * ======================================================================= */
OpenPLC_KNX_t KNX;

/* =========================================================================
 * Singleton convenience wrapper
 * ======================================================================= */
OpenPLC_KNX_Class KNXHelper;

/* -------------------------------------------------------------------------
 * Prog-button EXTI interrupt service routine
 *
 * Called on both edges of PG9.  The KnxFacade debounces internally via
 * the PROG_BTN_PRESS_MIN/MAX_MILLIS window (50–500 ms) only when
 * setButtonISRFunction is used.  Here we use toggleProgMode() directly
 * and rely on a 200 ms debounce guard.
 * ---------------------------------------------------------------------- */
void OpenPLC_KNX_Class::_progButtonISR()
{
    static uint32_t lastToggleMs = 0u;
    uint32_t now = HAL_GetTick();
    if ((now - lastToggleMs) > 200u) {
        KNX.toggleProgMode();
        lastToggleMs = now;
    }
}

/* -------------------------------------------------------------------------
 * Prog-LED callbacks - use HAL GPIO (not Arduino digitalWrite)
 * ---------------------------------------------------------------------- */
static void progLedOn()
{
    println("[LED] ON");
    HAL_GPIO_WritePin(KNX_PROG_LED_PORT, KNX_PROG_LED_PIN, GPIO_PIN_SET);
}

static void progLedOff()
{
    println("[LED] OFF");
    HAL_GPIO_WritePin(KNX_PROG_LED_PORT, KNX_PROG_LED_PIN, GPIO_PIN_RESET);
}

/* -------------------------------------------------------------------------
 * setup()
 * ---------------------------------------------------------------------- */
void OpenPLC_KNX_Class::setup(const char* serial, uint16_t mfr_id)
{
    /* Load application NVM; fall back to defaults if sector is blank */
    if (!knx_nvm_load(&_config)) {
        knx_nvm_set_defaults(&_config, KNX_DEFAULT_INDIVIDUAL_ADDR);
    }

    /* Enable GPIO clocks before any HAL_GPIO_Init calls */
    __HAL_RCC_GPIOG_CLK_ENABLE();   /* PG9  = prog-button, PG11 = prog-LED */
    __HAL_RCC_GPIOD_CLK_ENABLE();   /* PD7  = KNX_TP_OK                   */
    __HAL_RCC_GPIOH_CLK_ENABLE();   /* PH12 = KNX_TP_VCC_OK               */

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

    /* Configure prog-button pull-up first (stm32_interrupt_enable reads
     * the current PUPDR register to preserve it during GPIO re-init) */
    {
        GPIO_InitTypeDef init = {0};
        init.Pin   = KNX_PROG_KEY_PIN;
        init.Mode  = GPIO_MODE_INPUT;
        init.Pull  = GPIO_PULLUP;
        init.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(KNX_PROG_KEY_PORT, &init);
    }
    /* Register callback through STM32duino EXTI infrastructure so we do
     * not conflict with SrcWrapper's EXTI9_5_IRQHandler definition. */
    stm32_interrupt_enable(KNX_PROG_KEY_PORT, KNX_PROG_KEY_PIN,
                           OpenPLC_KNX_Class::_progButtonISR,
                           GPIO_MODE_IT_FALLING);

    /* Configure TP bus-OK and VCC-OK status inputs */
    {
        GPIO_InitTypeDef init = {0};
        init.Mode  = GPIO_MODE_INPUT;
        init.Pull  = GPIO_NOPULL;
        init.Speed = GPIO_SPEED_FREQ_LOW;
        init.Pin   = KNX_TP_OK_PIN;
        HAL_GPIO_Init(KNX_TP_OK_PORT, &init);
        init.Pin   = KNX_TP_VCC_OK_PIN;
        HAL_GPIO_Init(KNX_TP_VCC_OK_PORT, &init);
    }

    /* Configure KnxFacade: use callbacks for LED, bypass Arduino pin logic */
    KNX.ledPin(-1);                        /* disable Arduino LED control */
    KNX.buttonPin(-1);                     /* disable Arduino button control */
    KNX.setProgLedOnCallback(progLedOn);
    KNX.setProgLedOffCallback(progLedOff);

    /* Device identity */
    KNX.manufacturerId(mfr_id);
    KNX.bauNumber(KNX.platform().uniqueSerialNumber());

    /* Order / hardware type from serial string (padded to 10 bytes) */
    if (serial != nullptr) {
        uint8_t order[10u] = {0};
        size_t  len = strlen(serial);
        if (len > 10u) len = 10u;
        memcpy(order, serial, len);
        KNX.orderNumber(order);
    }

    KNX.version(0x0100u);
    KNX.individualAddress(_config.individual_addr);

    /* Load KNX stack NVM (ETS-programmed address table etc.)
     * This may restore a different individual address programmed by ETS,
     * so syncCemiClientAddress() must be called AFTER readMemory(). */
    KNX.readMemory();

    /* Sync app config IA from the authoritative KNX stack value.
     * If ETS programmed a new IA, _config.individual_addr was stale. */
    _config.individual_addr = KNX.individualAddress();

    /* Now that the final IA is known, synchronise the cEMI client address
     * (clientAddress = IA + 1 used by ETS tunnelling connection). */
    KNX.bau().syncCemiClientAddress();
}

/* -------------------------------------------------------------------------
 * start()
 * ---------------------------------------------------------------------- */
void OpenPLC_KNX_Class::start()
{
    KNX.start();
}

/* -------------------------------------------------------------------------
 * loop()
 * ---------------------------------------------------------------------- */
void OpenPLC_KNX_Class::loop()
{
    KNX.loop();
}

/* -------------------------------------------------------------------------
 * Application NVM
 * ---------------------------------------------------------------------- */
bool OpenPLC_KNX_Class::loadAppConfig()
{
    if (!knx_nvm_load(&_config)) {
        knx_nvm_set_defaults(&_config, KNX.individualAddress());
        return false;
    }
    return true;
}

bool OpenPLC_KNX_Class::saveAppConfig()
{
    _config.individual_addr = KNX.individualAddress();
    return knx_nvm_save(&_config);
}

/* -------------------------------------------------------------------------
 * Relay profile
 * ---------------------------------------------------------------------- */
bool OpenPLC_KNX_Class::initRelayProfile2CH()
{
    _relayProfile = knx_profile_relay_2ch();
    knx_profile_apply_defaults(_relayProfile, &_config);
    return true;
}

bool OpenPLC_KNX_Class::setRelayChannel(uint8_t channel, bool on)
{
    if (_relayProfile == nullptr) {
        println("[KNX] setRelayChannel: relay profile not initialised");
        return false;
    }
    if (!knx_profile_set_channel(_relayProfile, channel, on)) {
        print("[KNX] setRelayChannel: invalid channel ");
        println(channel);
        return false;
    }
    if (on)
        _config.last_relay_state_bits |=  (uint8_t)(1u << channel);
    else
        _config.last_relay_state_bits &= (uint8_t)~(1u << channel);
    return true;
}

bool OpenPLC_KNX_Class::relayChannelState(uint8_t channel) const
{
    if (_relayProfile == nullptr) return false;
    return knx_profile_channel_state(_relayProfile, channel);
}

/* -------------------------------------------------------------------------
 * Status GPIO
 * ---------------------------------------------------------------------- */
bool OpenPLC_KNX_Class::tpBusOk() const
{
    return HAL_GPIO_ReadPin(KNX_TP_OK_PORT, KNX_TP_OK_PIN) == GPIO_PIN_SET;
}

bool OpenPLC_KNX_Class::tpVccOk() const
{
    return HAL_GPIO_ReadPin(KNX_TP_VCC_OK_PORT, KNX_TP_VCC_OK_PIN) == GPIO_PIN_SET;
}

/* EXTI9_5_IRQHandler is provided by SrcWrapper (interrupt.cpp).
 * The prog-button callback is registered via stm32_interrupt_enable()
 * in setup(), which routes through SrcWrapper's HAL_GPIO_EXTI_Callback
 * dispatch - no custom ISR needed here. */

/* -------------------------------------------------------------------------
 * selfProgram2CH() - firmware-side KNX table initialisation
 *
 * Programs the four KNX system-B table objects (AddrTable, AssocTable,
 * GroupObjTable, AppProgram) directly via the property-write path so the
 * device becomes configured without an ETS application download.
 *
 * Call BEFORE the first call to KNX.configured() (e.g. right after
 * initRelayProfile2CH() and before the KNX.configured() check in setup).
 *
 * GroupObject mapping programmed:
 *   GO #1  DPT-1  GA 0/0/1 (0x0001)  → relay channel 0 (PI8)
 *   GO #2  DPT-1  GA 0/0/2 (0x0002)  → relay channel 1 (PI10)
 * ---------------------------------------------------------------------- */

/* Helper: run the KNX load sequence on one table object.
 *
 * Sequence:  START_LOADING → ADDITIONAL_LOAD_CONTROLS (allocate n bytes)
 *            → memcpy data into allocated buffer → LOAD_COMPLETED
 *
 * LOAD_COMPLETED triggers Memory::saveMemory() which persists the current
 * eeprom buffer to flash.  A final KNX.writeMemory() after all four tables
 * are loaded writes the proper header + saveRestore metadata so that
 * Memory::readMemory() can restore everything on next boot. */
static bool _selfLoadTable(InterfaceObject* obj, const uint8_t* src, uint32_t n)
{
    if (obj == nullptr || src == nullptr || n == 0u) return false;

    uint8_t buf[10];
    uint8_t cnt;

    /* 1. LE_START_LOADING */
    buf[0] = 1u;
    cnt = 1u;
    obj->writeProperty(PID_LOAD_STATE_CONTROL, 1u, buf, cnt);

    /* 2. LE_ADDITIONAL_LOAD_CONTROLS - Data Relative Allocation (0x0B) */
    buf[0] = 3u;                    /* LE_ADDITIONAL_LOAD_CONTROLS      */
    buf[1] = 0x0Bu;                 /* sub-opcode: Data Relative Alloc  */
    buf[2] = 0u;                    /* size high bytes (n < 256 here)   */
    buf[3] = 0u;
    buf[4] = 0u;
    buf[5] = (uint8_t)n;            /* size low byte                    */
    buf[6] = 0u;                    /* doFill = false                   */
    buf[7] = 0u;                    /* fillByte                         */
    cnt = 1u;
    obj->writeProperty(PID_LOAD_STATE_CONTROL, 1u, buf, cnt);

    /* 3. Write binary table data into the allocated RAM buffer */
    TableObject* tbl = static_cast<TableObject*>(obj);
    uint8_t* dest = tbl->tableData();
    if (dest == nullptr) return false;
    memcpy(dest, src, n);

    /* 4. LE_LOAD_COMPLETED - table transitions to LS_LOADED; flash saved */
    buf[0] = 2u;
    cnt = 1u;
    obj->writeProperty(PID_LOAD_STATE_CONTROL, 1u, buf, cnt);

    return true;
}

bool OpenPLC_KNX_Class::selfProgram2CH(uint16_t ia)
{
    /* Check address table load state - skip if already programmed. */
    InterfaceObject* addrObj = KNX.getInterfaceObject(1u); /* idx=1 = AddrTable */
    if (addrObj == nullptr) return false;
    {
        uint8_t state = 0u;
        uint8_t cnt   = 1u;
        addrObj->readProperty(PID_LOAD_STATE_CONTROL, 1u, cnt, &state);
        if (state == 1u /* LS_LOADED */) return true;  /* already done */
    }

    /* Set individual address */
    KNX.individualAddress(ia);

    /* --- Address Table (idx=1) ---
     *   Word[0] = count = 2
     *   Word[1] = GA 0/0/1 = 0x0001
     *   Word[2] = GA 0/0/2 = 0x0002
     * All big-endian (KNX network byte order). */
    static const uint8_t addrData[] = {
        0x00u, 0x02u,   /* count = 2            */
        0x00u, 0x01u,   /* GA #1 = 0/0/1        */
        0x00u, 0x02u    /* GA #2 = 0/0/2        */
    };
    if (!_selfLoadTable(KNX.getInterfaceObject(1u), addrData, sizeof(addrData)))
        return false;

    /* --- Association Table (idx=2) ---
     *   Word[0] = count = 2
     *   Word[1] = TSAP 1  (→ GA #1)
     *   Word[2] = ASAP 1  (→ GO #1)
     *   Word[3] = TSAP 2  (→ GA #2)
     *   Word[4] = ASAP 2  (→ GO #2) */
    static const uint8_t assocData[] = {
        0x00u, 0x02u,               /* count = 2            */
        0x00u, 0x01u, 0x00u, 0x01u, /* TSAP=1 → ASAP=1     */
        0x00u, 0x02u, 0x00u, 0x02u  /* TSAP=2 → ASAP=2     */
    };
    if (!_selfLoadTable(KNX.getInterfaceObject(2u), assocData, sizeof(assocData)))
        return false;

    /* --- Group Object Table (idx=3) ---
     *   Word[0] = count = 2
     *   Word[1] = GO #1 flags (see group_object.cpp for bit layout)
     *   Word[2] = GO #2 flags: same
     *
     *   0x1700 = 0001 0111 0000 0000
     *             bit15=responseUpdate=0  bit14=transmit=0
     *             bit13=valueReadOnInit=0  bit12=write=1
     *             bit11=read=0  bit10=comm=1
     *             bits9:6 = priority: (0x1700 >> 6) & 0x0C = 0x0C = LowPriority
     *             bits5:0 = size code 0 → 1-bit DPT-1
     *
     * Note: read (bit11) is disabled - device will not respond to GroupValueRead.
     * Enable bit11 (→ 0x1F00) if read-back from ETS is required. */
    static const uint8_t goData[] = {
        0x00u, 0x02u,   /* count = 2    */
        0x17u, 0x00u,   /* GO #1        */
        0x17u, 0x00u    /* GO #2        */
    };
    if (!_selfLoadTable(KNX.getInterfaceObject(3u), goData, sizeof(goData)))
        return false;

    /* --- Application Program (idx=4) - 1-byte placeholder, just needs LS_LOADED */
    static const uint8_t appData[] = {0x00u};
    if (!_selfLoadTable(KNX.getInterfaceObject(4u), appData, sizeof(appData)))
        return false;

    /* Serialize the full NVM state (header + saveRestores + table references)
     * to the eeprom buffer and commit to flash.  After this, readMemory() on
     * the next boot will restore all four tables to LS_LOADED. */
    KNX.writeMemory();

    return true;
}
