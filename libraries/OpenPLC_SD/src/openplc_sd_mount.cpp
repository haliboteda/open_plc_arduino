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

bool SDClass::begin(uint32_t detect, uint32_t level)
{
    if (_mounted) {
        end();
    }
    _detect = detect;
    _level = level;
    uint32_t pin = (detect == OPENPLC_SD_NO_DETECT) ? SD_DETECT_NONE : detect;
    if (!s_card.init(pin, level)) {
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
