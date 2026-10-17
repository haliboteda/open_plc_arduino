/*
 * T3-22: after a KNX role change the stack restores nothing from the data area.
 *
 *   knx_role_test <scenario>
 *
 * Real knx/memory.cpp + knx/device_object.cpp over a RAM data area. A data
 * area is "written by role X" when a DeviceObject carrying role X in its
 * hardwareType goes through Memory::writeMemory(); a boot is a fresh
 * DeviceObject/Memory pair with the booting role, then readMemory().
 * torn_write: real knx_nvm.cpp over a fake flash driver (stubs/stm32h7xx_hal.h)
 * that can cut power after N flash-word writes.
 * Requirement: $PROD/docs/modules/M3-app-runtime.md, R3-20 / T3-22.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "knx/memory.h"
#include "knx/device_object.h"
#include "knx/platform.h"
#include "knx_config.h"
#include "knx_nvm.h"

#define ROLE_TP      0x07B0u  /* TP device */
#define ROLE_ROUTER  0x091Au  /* IP router */
#define ROLE_LEGACY  0x0000u  /* header written before 2026-10-16: no role */

static const uint16_t MFR_ID = 0x00FA;
static const uint16_t APP_VERSION = 0x0100;      /* OpenPLC_KNX.cpp setup() */
static const uint16_t DEFAULT_IA = 0x1101;       /* KNX_DEFAULT_INDIVIDUAL_ADDR, knx_config.h */
static const uint16_t PROGRAMMED_IA = 0x1105;    /* what ETS downloaded */
static const uint8_t  PROGRAMMED_HOPS = 3;       /* default is 6 */

static int failures;

static void check(bool ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) {
        failures++;
    }
}

uint32_t millis() { return 0; }

/* --- data area in RAM ----------------------------------------------------- */

static uint8_t nvm[KNX_FLASH_SIZE];

class RamPlatform : public Platform
{
    public:
        int fatal = 0;
        void restart() override {}
        void fatalError() override { fatal++; }
        uint8_t* getEepromBuffer(uint32_t) override { return nvm; }
};

/* Stands for the role's other saved objects; the router saves more than the
 * TP device, so the two roles' layouts after the device object differ. */
class Probe : public SaveRestore
{
    public:
        explicit Probe(uint16_t mask) : _size(mask == ROLE_ROUTER ? 40 : 8) {}
        uint8_t* save(uint8_t* buffer) override
        {
            memset(buffer, 0xA5, _size);
            return buffer + _size;
        }
        const uint8_t* restore(const uint8_t* buffer) override
        {
            restores++;
            intact = true;
            for (uint16_t i = 0; i < _size; i++) {
                intact = intact && buffer[i] == 0xA5;
            }
            return buffer + _size;
        }
        uint16_t saveSize() override { return _size; }
        int restores = 0;
        bool intact = false;
    private:
        uint16_t _size;
};

/* Identity as OpenPLC_KNX.cpp setup() sets it before readMemory(); the
 * hardwareType lines are the same as there. */
static void set_identity(DeviceObject& dev, uint16_t mask)
{
    dev.manufacturerId(MFR_ID);
    dev.version(APP_VERSION);
    uint8_t hw[LEN_HARDWARE_TYPE] = {0};
    hw[0] = (uint8_t)((mask >> 8) & 0xFFu);
    hw[1] = (uint8_t)(mask & 0xFFu);
    dev.hardwareType(hw);
    dev.individualAddress(DEFAULT_IA);
}

/* An ETS download by role `mask`, saved to the data area. */
static void program(uint16_t mask)
{
    memset(nvm, 0xFF, sizeof(nvm));
    RamPlatform plat;
    DeviceObject dev;
    Memory mem(plat, dev);
    Probe probe(mask);
    mem.addSaveRestore(&dev);
    mem.addSaveRestore(&probe);
    set_identity(dev, mask);
    dev.individualAddress(PROGRAMMED_IA);
    dev.property(PID_ROUTING_COUNT)->write((uint8_t)(PROGRAMMED_HOPS << 4));
    mem.writeMemory();
}

struct BootResult
{
    uint16_t ia;
    uint8_t hops;
    int restores;
    bool intact;
    bool area_unchanged;
    int fatal;
};

static BootResult boot(uint16_t mask)
{
    static uint8_t before[sizeof(nvm)];
    memcpy(before, nvm, sizeof(nvm));

    RamPlatform plat;
    DeviceObject dev;
    Memory mem(plat, dev);
    Probe probe(mask);
    mem.addSaveRestore(&dev);
    mem.addSaveRestore(&probe);
    set_identity(dev, mask);
    mem.readMemory();

    BootResult r;
    r.ia = dev.individualAddress();
    r.hops = dev.defaultHopCount();
    r.restores = probe.restores;
    r.intact = probe.intact;
    r.area_unchanged = memcmp(before, nvm, sizeof(nvm)) == 0;
    r.fatal = plat.fatal;
    return r;
}

/* --- fake flash: the KNX sector's first KNX_FLASH_SIZE bytes in RAM ---------- */

static const uint32_t FLASH_WORD = 32;
static uint8_t flash_img[KNX_FLASH_SIZE];
static int programs;          /* successful flash-word writes since the erase */
static int cut_after = -1;    /* power cut after this many writes; -1 = never */
static uint32_t last_addr;
static bool erase_ok;
static bool locked = true;

HAL_StatusTypeDef HAL_FLASH_Unlock(void) { locked = false; return HAL_OK; }
HAL_StatusTypeDef HAL_FLASH_Lock(void) { locked = true; return HAL_OK; }

HAL_StatusTypeDef HAL_FLASHEx_Erase(FLASH_EraseInitTypeDef *init, uint32_t *sectorError)
{
    erase_ok = !locked && init->TypeErase == FLASH_TYPEERASE_SECTORS
            && init->VoltageRange == FLASH_VOLTAGE_RANGE_3
            && init->Banks == FLASH_BANK_2 && init->Sector == FLASH_SECTOR_6
            && init->NbSectors == 1u;
    *sectorError = 0xFFFFFFFFu;
    memset(flash_img, 0xFF, sizeof(flash_img));
    programs = 0;
    return erase_ok ? HAL_OK : HAL_ERROR;
}

HAL_StatusTypeDef HAL_FLASH_Program(uint32_t typeProgram, uint32_t flashAddress, uint32_t dataAddress)
{
    if (cut_after >= 0 && programs >= cut_after) {
        return HAL_ERROR;
    }
    uint32_t off = flashAddress - KNX_STACK_NVM_FLASH_ADDR;
    if (locked || typeProgram != FLASH_TYPEPROGRAM_FLASHWORD
        || flashAddress < KNX_STACK_NVM_FLASH_ADDR || off % FLASH_WORD != 0
        || off + FLASH_WORD > sizeof(flash_img)) {
        printf("FAIL  bad flash program at 0x%08X\n", (unsigned)flashAddress);
        exit(1);
    }
    /* The driver passes the source as a 32-bit address; on a 64-bit host the
     * upper half is restored from this frame, since the source is on the stack. */
    uint8_t here;
    uintptr_t at = (uintptr_t)&here;
    int32_t delta = (int32_t)(dataAddress - (uint32_t)at);
    if (delta > 0x100000 || delta < -0x100000) {
        printf("FAIL  source 0x%08X is not on the stack\n", (unsigned)dataAddress);
        exit(1);
    }
    memcpy(flash_img + off, (const void *)(at + (intptr_t)delta), FLASH_WORD);
    last_addr = flashAddress;
    programs++;
    return HAL_OK;
}

/* --- scenarios -------------------------------------------------------------- */

static void expect_restored(uint16_t mask, const char *role)
{
    program(mask);
    BootResult r = boot(mask);
    printf("-- %s data area, %s boots\n", role, role);
    check(r.ia == PROGRAMMED_IA, "the programmed individual address is restored");
    check(r.hops == PROGRAMMED_HOPS, "a saved device object property is restored");
    check(r.restores == 1 && r.intact, "the role's other saved objects are restored intact");
    check(r.fatal == 0, "no fatal error");
}

static void expect_discarded(uint16_t booted_as, const char *what)
{
    BootResult r = boot(booted_as);
    printf("-- %s\n", what);
    check(r.ia == DEFAULT_IA, "the individual address stays at the default");
    check(r.hops == 6, "device object properties keep their defaults");
    check(r.restores == 0, "no other saved object is restored (nothing read past the header)");
    check(r.area_unchanged, "the data area is left as it was");
    check(r.fatal == 0, "no fatal error");
}

static void t_same_tp() { expect_restored(ROLE_TP, "TP device"); }
static void t_same_router() { expect_restored(ROLE_ROUTER, "IP router"); }

static void t_tp_to_router()
{
    program(ROLE_TP);
    expect_discarded(ROLE_ROUTER, "TP device data area, IP router boots");
}

static void t_router_to_tp()
{
    program(ROLE_ROUTER);
    expect_discarded(ROLE_TP, "IP router data area, TP device boots");
}

static void t_legacy()
{
    program(ROLE_LEGACY);
    expect_discarded(ROLE_TP, "header without role (hardwareType all 0), TP device boots");
    expect_discarded(ROLE_ROUTER, "header without role (hardwareType all 0), IP router boots");
}

static bool header_erased()
{
    for (uint32_t i = 0; i < FLASH_WORD; i++) {
        if (flash_img[i] != 0xFF) {
            return false;
        }
    }
    return true;
}

static void t_torn_write()
{
    /* No word all 0xFF, so every word is one program call. */
    static uint8_t data[KNX_FLASH_SIZE];
    for (uint32_t i = 0; i < sizeof(data); i++) {
        data[i] = (uint8_t)(i * 7u + 1u) == 0xFF ? 0x00 : (uint8_t)(i * 7u + 1u);
    }
    const int words = (int)(sizeof(data) / FLASH_WORD);

    printf("-- power cut after each of %d flash-word writes\n", words);
    bool all_erased = true, all_failed = true;
    for (int n = 0; n < words; n++) {
        cut_after = n;
        bool ok = knx_nvm_sector_write(data, sizeof(data));
        all_failed = all_failed && !ok && locked;
        if (!header_erased()) {
            printf("      header written with the cut after %d writes\n", n);
            all_erased = false;
        }
    }
    check(all_erased, "the header word is still 0xFF after every cut");
    check(all_failed, "every cut write reports failure and relocks the flash");

    printf("-- no power cut\n");
    cut_after = -1;
    bool ok = knx_nvm_sector_write(data, sizeof(data));
    check(ok && erase_ok && locked, "the write succeeds, erasing only the KNX sector");
    check(memcmp(flash_img, data, sizeof(data)) == 0, "the sector holds the whole block");
    check(programs == words && last_addr == KNX_STACK_NVM_FLASH_ADDR,
          "the header word is the last one written");

    /* A real TP data area cut at its last write: the stack restores nothing. */
    program(ROLE_TP);
    cut_after = -1;
    knx_nvm_sector_write(nvm, sizeof(nvm));
    cut_after = programs - 1;
    knx_nvm_sector_write(nvm, sizeof(nvm));
    memcpy(nvm, flash_img, sizeof(nvm));
    cut_after = -1;
    expect_discarded(ROLE_TP, "TP device data area cut before its header word, TP device boots");
}

int main(int argc, char **argv)
{
    const char *s = (argc > 1) ? argv[1] : "";

    if      (strcmp(s, "same_tp") == 0)      { t_same_tp(); }
    else if (strcmp(s, "same_router") == 0)  { t_same_router(); }
    else if (strcmp(s, "tp_to_router") == 0) { t_tp_to_router(); }
    else if (strcmp(s, "router_to_tp") == 0) { t_router_to_tp(); }
    else if (strcmp(s, "legacy") == 0)       { t_legacy(); }
    else if (strcmp(s, "torn_write") == 0)   { t_torn_write(); }
    else {
        printf("unknown scenario \"%s\"\n", s);
        return 2;
    }
    return failures ? 1 : 0;
}
