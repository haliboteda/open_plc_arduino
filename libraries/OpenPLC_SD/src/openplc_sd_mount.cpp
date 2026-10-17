/*
 * openplc_sd_mount.cpp -- SD.begin() / SD.end(): the SDMMC driver and the
 * FatFs volume, from STM32SD's BSD-3 Sd2Card and SdFatFs.
 */
#include <Arduino.h>   /* the driver headers expect it first */
#include "Sd2Card.h"
#include "SdFatFs.h"
#include "OpenPLC_SD.h"

static Sd2Card s_card;
static SdFatFs s_fs;

bool SDClass::begin()
{
    if (_mounted) {
        end();
    }
    if (!s_card.init(SDMMC_CD, LOW)) {
        return false;
    }
    if (!s_fs.init()) {
        s_card.deinit();
        return false;
    }
    _mounted = true;
    return true;
}

bool SDClass::end()
{
    if (!_mounted) {
        return true;
    }
    _mounted = false;
    bool ok = s_fs.deinit();
    return s_card.deinit() && ok;
}
