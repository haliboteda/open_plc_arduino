#pragma once

/*
 * OpenPLC_KNX.h — Public API for KNX communication on OpenPLC STM32H743 boards.
 *
 * This is a thin facade over the thelsing/knx reference library.
 * By default it uses KnxFacade<Stm32H743OpenPLCPlatform, Bau57B0>
 * (mask version 0x57B0: KNXnet/IP device). The sketch/build may override
 * MASK_VERSION when TP-only or IP/TP coupler behaviour is required.
 *
 * Usage (Arduino sketch):
 *
 *   #include <OpenPLC_KNX.h>
 *
 *   void onSwitch(GroupObject& go) {
 *       bool on = go.value<bool>();
 *       digitalWrite(LED_BUILTIN, on);
 *   }
 *
 *   void setup() {
 *       KNX.setup("OPENPLC000001");   // serial number / ETS identifier
 *       GroupObject& go = KNX.getGroupObject(0);  // index 0 in ETS table
 *       go.callback(onSwitch);
 *       KNX.start();
 *   }
 *
 *   void loop() {
 *       KNX.loop();
 *   }
 *
 * Programming mode button / LED are handled internally (HAL EXTI + GPIO).
 * Press the button on PG9 to toggle ETS programming mode.
 */

/* Pull in MASK_VERSION + KNX_NO_AUTOMATIC_GLOBAL_INSTANCE before any
 * reference-library header — they are defined in the platform header. */
#include "stm32h743_openplc_platform.h"

/* Reference library public headers */
#include <knx.h>                        /* KnxFacade + all BAU headers  */
#include <knx/group_object.h>           /* GroupObject class            */

/* Application-layer headers (HAL-only, no Arduino) */
#include "knx_nvm.h"
#include "knx_profiles.h"

/* -------------------------------------------------------------------------
 * Global KNX instance type alias — selected by MASK_VERSION at compile time.
 *
 * MASK_VERSION is set in stm32h743_openplc_platform.h (default 0x5780) or
 * may be overridden per-sketch via a -D compiler flag or board variant.
 *
 *   0x5780  IP+TP dual device  (DEFAULT) — both transports, group objects ✓
 *   0x07B0  KNX TP device      — TP only, group objects ✓
 *   0x57B0  KNXnet/IP device   — IP only, group objects ✓
 *   0x091A  IP/TP coupler      — both transports, routes between them, no local GOs
 *
 * For 0x5780, 0x07B0, 0x57B0: KNX.getGroupObject(n) is available.
 * For 0x091A (coupler): application group objects are not supported.
 * ---------------------------------------------------------------------- */
#if   MASK_VERSION == 0x5780
  using OpenPLC_KNX_t = KnxFacade<Stm32H743OpenPLCPlatform, Bau5780>;
#elif MASK_VERSION == 0x07B0
  using OpenPLC_KNX_t = KnxFacade<Stm32H743OpenPLCPlatform, Bau07B0>;
#elif MASK_VERSION == 0x57B0
  using OpenPLC_KNX_t = KnxFacade<Stm32H743OpenPLCPlatform, Bau57B0>;
#elif MASK_VERSION == 0x091A
  using OpenPLC_KNX_t = KnxFacade<Stm32H743OpenPLCPlatform, Bau091A>;
#else
  #error "OpenPLC_KNX: unsupported MASK_VERSION. Use 0x5780 (IP+TP, default), 0x07B0 (TP), 0x57B0 (IP), or 0x091A (coupler)."
#endif

/* -------------------------------------------------------------------------
 * Singleton — declared here, defined in OpenPLC_KNX.cpp
 * ---------------------------------------------------------------------- */
extern OpenPLC_KNX_t KNX;

/* -------------------------------------------------------------------------
 * Convenience wrapper
 *
 * Provides the same simple API as before for sketches that do not need
 * direct access to the KnxFacade internals.
 * ---------------------------------------------------------------------- */
class OpenPLC_KNX_Class
{
public:
    /* --- Initialisation ----------------------------------------------- */

    /* Call once in setup().
     *   serial     — 12-char device serial number shown in ETS (e.g. "OPENPLC000001")
     *   mfr_id     — KNX manufacturer ID (default 0x00FA = Weinzierl Engineering)
     * Configures prog-button interrupt on PG9 and prog-LED on PG11.
     * Does NOT start the stack — call start() after registering group objects. */
    void setup(const char* serial = "OPENPLC000001",
               uint16_t    mfr_id = 0x00FAu);

    /* Register all group objects (via KNX.bau().groupObjectTableObject()),
     * then call start() to enable the stack and transports. */
    void start();

    /* Call every iteration of loop(). */
    void loop();

    /* --- Application NVM (relay channels etc.) ------------------------ */
    bool loadAppConfig();
    bool saveAppConfig();
    KnxNvmConfig* appConfig() { return &_config; }

    /* --- Relay profile helpers ---------------------------------------- */
    bool initRelayProfile2CH();
    bool setRelayChannel(uint8_t channel, bool on);
    bool relayChannelState(uint8_t channel) const;

    /* --- Firmware self-programming (bypass ETS for bench testing) ------- */
    /* Programs the KNX address/association/group-object/app-program tables
     * directly into NVM so that KNX.configured() returns true without an ETS
     * application download.  Must be called BEFORE the first call to
     * KNX.configured().  Persists to flash; survives power cycles.
     *
     * Table layout programmed:
     *   GO #1  DPT-1  group addr 0/0/1  → relay channel 0
     *   GO #2  DPT-1  group addr 0/0/2  → relay channel 1
     *
     * Returns true on success. */
    bool selfProgram2CH(uint16_t ia = KNX_DEFAULT_INDIVIDUAL_ADDR);

    /* --- Status -------------------------------------------------------- */
    bool tpBusOk()   const;  /* STKNX bus-OK GPIO (PD7) */
    bool tpVccOk()   const;  /* STKNX VCC-OK GPIO (PH12) */
    bool progMode()  const  { return KNX.progMode(); }

    /* Prog-button ISR thunk — static so it can be called from the C
     * EXTI9_5_IRQHandler.  Must be public so the extern "C" handler can
     * reach it without a friend declaration. */
    static void _progButtonISR();

private:
    KnxNvmConfig             _config;
    const KnxRelayProfile   *_relayProfile = nullptr;
};

extern OpenPLC_KNX_Class KNXHelper;
