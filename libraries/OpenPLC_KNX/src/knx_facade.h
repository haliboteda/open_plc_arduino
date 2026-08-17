#pragma once

/*
 * knx_facade.h - stripped for STM32H743 / OpenPLC only.
 *
 * All non-STM32H743 platform branches (ESP32, RP2040, SAMD, Linux, CC1310,
 * LibreTiny …) have been removed.  The only supported configuration is:
 *
 *   Platform : Stm32H743OpenPLCPlatform   (inherits Platform directly)
 *   BAU      : Bau07B0  (MASK_VERSION 0x07B0, TP device)
 *           or Bau57B0  (MASK_VERSION 0x57B0, IP device)
 *           or Bau091A  (MASK_VERSION 0x091A, IP/TP1 coupler)
 *
 * KNX_NO_AUTOMATIC_GLOBAL_INSTANCE is always defined - the global KNX
 * instance is created explicitly in OpenPLC_KNX.cpp.
 */

/* Bug 1 fix: MASK_VERSION must be defined before knx/config.h checks it.
 * stm32h743_openplc_platform.h (included below) also sets this default,
 * but it comes after the config.h include.  Set it here so a direct
 * #include "knx_facade.h" also works without the platform header first. */
#ifndef MASK_VERSION
#  define MASK_VERSION 0x5780u   /* IP+TP dual device - both transports active */
#endif

#include "knx/bits.h"
#include "knx/config.h"
#include "knx/bau07B0.h"
#include "knx/bau091A.h"
#include "knx/bau57B0.h"
#include "knx/bau5780.h"

/* Our platform - already defines KNX_NO_AUTOMATIC_GLOBAL_INSTANCE and
 * MASK_VERSION if not set by the user.  Include it exactly once here. */
#include "stm32h743_openplc_platform.h"

#ifndef USERDATA_SAVE_SIZE
    #define USERDATA_SAVE_SIZE 0
#endif

#ifndef KNX_LED
    #define KNX_LED -1
#endif
#ifndef KNX_LED_ACTIVE_ON
    #define KNX_LED_ACTIVE_ON 0
#endif
#ifndef KNX_BUTTON
    #define KNX_BUTTON -1
#endif

typedef const uint8_t* (*RestoreCallback)(const uint8_t* buffer);
typedef uint8_t* (*SaveCallback)(uint8_t* buffer);
typedef void (*IsrFunctionPtr)();
typedef void (*ProgLedOnCallback)();
typedef void (*ProgLedOffCallback)();
#ifdef KNX_ACTIVITYCALLBACK
    typedef void (*ActivityCallback)(uint8_t info);
#endif

template <class P, class B> class KnxFacade : private SaveRestore
{
    public:
        KnxFacade() : _platformPtr(new P()), _bauPtr(new B(*_platformPtr)), _bau(*_bauPtr)
        {
            manufacturerId(0xfa);
            bauNumber(platform().uniqueSerialNumber());
            _bau.addSaveRestore(this);
        }

        KnxFacade(B& bau) : _bau(bau)
        {
            _platformPtr = static_cast<P*>(&bau.platform());
            manufacturerId(0xfa);
            bauNumber(platform().uniqueSerialNumber());
            _bau.addSaveRestore(this);
        }

        KnxFacade(IsrFunctionPtr buttonISRFunction) : _platformPtr(new P()), _bauPtr(new B(*_platformPtr)), _bau(*_bauPtr)
        {
            manufacturerId(0xfa);
            bauNumber(platform().uniqueSerialNumber());
            _bau.addSaveRestore(this);
            setButtonISRFunction(buttonISRFunction);
        }

        virtual ~KnxFacade()
        {
            if (_bauPtr)
                delete _bauPtr;

            if (_platformPtr)
                delete _platformPtr;
        }

        P& platform()
        {
            return *_platformPtr;
        }

        B& bau()
        {
            return _bau;
        }

        bool enabled()
        {
            return _bau.enabled();
        }

        void enabled(bool value)
        {
            _bau.enabled(value);
        }

        bool progMode()
        {
            return _bau.deviceObject().progMode();
        }

        void progMode(bool value)
        {
            _bau.deviceObject().progMode(value);
        }

        void toggleProgMode()
        {
            _toggleProgMode = true;
        }

        bool configured()
        {
            return _bau.configured();
        }

        uint32_t ledPinActiveOn()
        {
            return _ledPinActiveOn;
        }

        void ledPinActiveOn(uint32_t value)
        {
            _ledPinActiveOn = value;
        }

        int32_t ledPin()
        {
            return _ledPin;
        }

        void ledPin(int32_t value)
        {
            _ledPin = value;
        }

        void setProgLedOffCallback(ProgLedOffCallback progLedOffCallback)
        {
            _progLedOffCallback = progLedOffCallback;
        }

        void setProgLedOnCallback(ProgLedOnCallback progLedOnCallback)
        {
            _progLedOnCallback = progLedOnCallback;
        }

        int32_t buttonPin()
        {
            return _buttonPin;
        }

        void buttonPin(int32_t value)
        {
            _buttonPin = value;
        }

        void readMemory()
        {
            _bau.readMemory();
        }

        void writeMemory()
        {
            _bau.writeMemory();
        }

        uint16_t individualAddress()
        {
            return _bau.deviceObject().individualAddress();
        }

        void individualAddress(uint16_t addr)
        {
            _bau.deviceObject().individualAddress(addr);
        }

        void loop()
        {
            if (progMode() != _progLedState)
            {
                _progLedState = progMode();
                if (_progLedState)
                    progLedOn();
                else
                    progLedOff();
            }

            if (_toggleProgMode)
            {
                progMode(!progMode());
                _toggleProgMode = false;
            }

            _bau.loop();
        }

        void manufacturerId(uint16_t value)
        {
            _bau.deviceObject().manufacturerId(value);
        }

        void bauNumber(uint32_t value)
        {
            _bau.deviceObject().bauNumber(value);
        }

        void orderNumber(const uint8_t* value)
        {
            _bau.deviceObject().orderNumber(value);
        }

        void hardwareType(const uint8_t* value)
        {
            _bau.deviceObject().hardwareType(value);
        }

        void version(uint16_t value)
        {
            _bau.deviceObject().version(value);
        }

        /* start() - sets up LED pin if ledPin >= 0, button pin if buttonPin >= 0.
         * For STM32H743 we set both to -1 and use HAL callbacks instead,
         * so this reduces to enabled(true). */
        void start()
        {
            /* LED and button GPIO are managed by OpenPLC_KNX_Class::setup()
             * via HAL directly.  _ledPin and _buttonPin are -1 here. */
            progLedOff();
            enabled(true);
        }

        void setButtonISRFunction(IsrFunctionPtr progButtonISRFuncPtr)
        {
            _progButtonISRFuncPtr = progButtonISRFuncPtr;
        }

        void setSaveCallback(SaveCallback func)
        {
            _saveCallback = func;
        }

        void setRestoreCallback(RestoreCallback func)
        {
            _restoreCallback = func;
        }

        uint8_t* paramData(uint32_t addr)
        {
            if (!_bau.configured())
                return nullptr;

            return _bau.parameters().data(addr);
        }

        bool paramBit(uint32_t addr, uint8_t shift)
        {
            if (!_bau.configured())
                return 0;

            return (bool)((_bau.parameters().getByte(addr) >> (7 - shift)) & 0x01);
        }

        uint8_t paramByte(uint32_t addr)
        {
            if (!_bau.configured())
                return 0;

            return _bau.parameters().getByte(addr);
        }

        int8_t paramSignedByte(uint32_t addr)
        {
            if (!_bau.configured())
                return 0;

            return (int8_t)_bau.parameters().getByte(addr);
        }

        uint16_t paramWord(uint32_t addr)
        {
            if (!_bau.configured())
                return 0;

            return _bau.parameters().getWord(addr);
        }

        uint32_t paramInt(uint32_t addr)
        {
            if (!_bau.configured())
                return 0;

            return _bau.parameters().getInt(addr);
        }

        double paramFloat(uint32_t addr, ParameterFloatEncodings enc)
        {
            if (!_bau.configured())
                return 0;

            return _bau.parameters().getFloat(addr, enc);
        }

/* getGroupObject is only valid for application devices (TP, IP, or both).
 * Bau091A (coupler) inherits BauSystemBCoupler which has no GroupObjectTable.
 *
 * 0x5780 was missing from this list until 2026-08-17, and it is the DEFAULT
 * knxrole (dual_device, "IP+TP Device"). Bau5780 derives from BauSystemBDevice
 * exactly like Bau07B0 and Bau57B0 do, so it has a group object table and
 * always did -- only this guard disagreed. The effect was that the default KNX
 * configuration could not use group objects at all, which is most of what KNX
 * application code does, and two of this library's own examples (KNX_Basic,
 * KNX_IP_Test) did not compile with the default FQBN.
 *
 * Found by TestTool/host/examples_build, which builds every example. */
#if (MASK_VERSION == 0x07B0) || (MASK_VERSION == 0x57B0) || (MASK_VERSION == 0x5780)
        GroupObject& getGroupObject(uint16_t goNr)
        {
            return _bau.groupObjectTable().get(goNr);
        }
#endif

        void restart(uint16_t individualAddress)
        {
            SecurityControl sc = {false, None};
            _bau.restartRequest(individualAddress, sc);
        }

        void beforeRestartCallback(BeforeRestartCallback func)
        {
            _bau.beforeRestartCallback(func);
        }

        BeforeRestartCallback beforeRestartCallback()
        {
            return _bau.beforeRestartCallback();
        }

        /* Expose interface object lookup - used by selfProgram helpers to access
         * table objects (AddrTable=1, AssocTable=2, GroupObjTable=3, AppProg=4). */
        InterfaceObject* getInterfaceObject(uint8_t idx)
        {
            return _bau.getInterfaceObject(idx);
        }

    private:
        P* _platformPtr = 0;
        B* _bauPtr = 0;
        B& _bau;
        ProgLedOnCallback  _progLedOnCallback  = 0;
        ProgLedOffCallback _progLedOffCallback = 0;
#ifdef KNX_ACTIVITYCALLBACK
        ActivityCallback _activityCallback = 0;
#endif
        uint32_t _ledPinActiveOn = KNX_LED_ACTIVE_ON;
        int32_t  _ledPin         = KNX_LED;
        int32_t  _buttonPin      = KNX_BUTTON;
        SaveCallback    _saveCallback    = 0;
        RestoreCallback _restoreCallback = 0;
        volatile bool _toggleProgMode = false;
        bool          _progLedState   = false;
        uint16_t      _saveSize       = USERDATA_SAVE_SIZE;
        IsrFunctionPtr _progButtonISRFuncPtr = 0;

        uint8_t* save(uint8_t* buffer)
        {
            if (_saveCallback != 0)
                return _saveCallback(buffer);
            return buffer;
        }

        const uint8_t* restore(const uint8_t* buffer)
        {
            if (_restoreCallback != 0)
                return _restoreCallback(buffer);
            return buffer;
        }

        uint16_t saveSize()  { return _saveSize; }
        void saveSize(uint16_t size) { _saveSize = size; }

        void progLedOn()
        {
            if (_progLedOnCallback != 0)
                _progLedOnCallback();
        }

        void progLedOff()
        {
            if (_progLedOffCallback != 0)
                _progLedOffCallback();
        }
};

/* No automatic global instance - OpenPLC_KNX.cpp creates it explicitly. */
