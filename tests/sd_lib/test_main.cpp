/*
 * T3-12: OpenPLC_SD's file operations, on the real FatFs over a RAM disk.
 *
 *   sd_lib_test <scenario>
 *
 * SD.begin() here formats and mounts the RAM disk; on the board it is
 * openplc_sd_mount.cpp, which this test replaces.
 */
#include "OpenPLC_SD.h"
#include "ff_gen_drv.h"

#include <set>
#include <string>

static int failures;

static void check(bool ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) {
        failures++;
    }
}

int digitalRead(uint32_t) { return LOW; }

/* --- a 4 MiB RAM disk ------------------------------------------------------ */

static const unsigned SECTORS = 8192;
static BYTE disk[SECTORS * 512];

static DSTATUS ram_init(BYTE) { return 0; }
static DSTATUS ram_status(BYTE) { return 0; }

static DRESULT ram_read(BYTE, BYTE *buf, LBA_t sector, UINT count)
{
    memcpy(buf, &disk[sector * 512], count * 512);
    return RES_OK;
}

static DRESULT ram_write(BYTE, const BYTE *buf, LBA_t sector, UINT count)
{
    memcpy(&disk[sector * 512], buf, count * 512);
    return RES_OK;
}

static DRESULT ram_ioctl(BYTE, BYTE cmd, void *buf)
{
    switch (cmd) {
    case GET_SECTOR_COUNT: *(LBA_t *)buf = SECTORS; return RES_OK;
    case GET_SECTOR_SIZE:  *(WORD *)buf = 512; return RES_OK;
    case GET_BLOCK_SIZE:   *(DWORD *)buf = 1; return RES_OK;
    case CTRL_SYNC:        return RES_OK;
    default:               return RES_PARERR;
    }
}

static const Diskio_drvTypeDef RAM_Driver = { ram_init, ram_status, ram_read, ram_write, ram_ioctl };
static char drive[4];
static FATFS volume;

bool SDClass::begin(uint32_t detect, uint32_t level)
{
    static bool formatted;
    _detect = detect;
    _level = level;
    if (!formatted) {
        static BYTE work[FF_MAX_SS];
        MKFS_PARM opt = { FM_ANY, 0, 0, 0, 0 };
        if (FATFS_LinkDriver(&RAM_Driver, drive) != 0 || f_mkfs(drive, &opt, work, sizeof(work)) != FR_OK) {
            return false;
        }
        formatted = true;
    }
    _mounted = f_mount(&volume, drive, 1) == FR_OK;
    return _mounted;
}

bool SDClass::end()
{
    _mounted = false;
    return f_unmount(drive) == FR_OK;
}

/* --- helpers --------------------------------------------------------------- */

static std::string slurp(const char *path)
{
    std::string out;
    File f = SD.open(path);
    if (!f) {
        return "<missing>";
    }
    int c;
    while ((c = f.read()) >= 0) {
        out += (char)c;
    }
    f.close();
    return out;
}

static void put(const char *path, const char *text)
{
    File f = SD.open(path, FILE_WRITE);
    f.print(text);
    f.close();
}

/* --- scenarios ------------------------------------------------------------- */

static void t_write_read()
{
    put("/T.TXT", "hello from OpenPLC");
    check(slurp("/T.TXT") == "hello from OpenPLC", "what was written reads back");
    File f = SD.open("/T.TXT");
    check(f.size() == 18 && f.available() == 18, "size and available match the bytes written");
    f.close();
}

static void t_append()
{
    put("/A.TXT", "ab");
    put("/A.TXT", "cd");
    check(slurp("/A.TXT") == "abcd", "FILE_WRITE on an existing file writes at its end");
}

static void t_seek_peek()
{
    put("/S.TXT", "abcdef");
    File f = SD.open("/S.TXT");
    check(f.seek(2) && f.peek() == 'c' && f.read() == 'c', "seek, then peek and read the same byte");
    check(f.position() == 3, "position follows the read");
    uint8_t buf[8];
    check(f.read(buf, sizeof(buf)) == 3 && memcmp(buf, "def", 3) == 0 && f.read() == -1,
          "a block read stops at the end, then read gives -1");
    f.close();
}

static void t_remove_exists()
{
    put("/R.TXT", "x");
    check(SD.exists("/R.TXT"), "a written file exists");
    check(SD.remove("/R.TXT") && !SD.exists("/R.TXT"), "remove deletes it");
    check(!SD.open("/R.TXT"), "opening a missing file for reading fails");
}

static void t_dirs()
{
    check(SD.mkdir("/D"), "mkdir");
    put("/D/A.TXT", "1");
    put("/D/a long file name.txt", "2");
    File d = SD.open("/D");
    check(d && d.isDirectory(), "a directory opens as one");
    std::set<std::string> names;
    for (File e = d.openNextFile(); e; e = d.openNextFile()) {
        names.insert(e.name());
        e.close();
    }
    check(names == std::set<std::string>{"A.TXT", "a long file name.txt"},
          "openNextFile lists each entry once, long names included");
    d.rewindDirectory();
    File again = d.openNextFile();
    check((bool)again, "rewindDirectory starts the listing over");
    again.close();
    d.close();
    check(!SD.rmdir("/D"), "a directory with files in it is not removed");
    SD.remove("/D/A.TXT");
    SD.remove("/D/a long file name.txt");
    check(SD.rmdir("/D") && !SD.exists("/D"), "an empty directory is removed");
}

static void t_root()
{
    put("/ROOT.TXT", "r");
    File root = SD.open("/");
    bool found = false;
    for (File e = root.openNextFile(); e; e = root.openNextFile()) {
        found = found || strcmp(e.name(), "ROOT.TXT") == 0;
        e.close();
    }
    root.close();
    check(found, "the root directory lists its files");
}

static void t_unmounted()
{
    SD.end();
    check(!SD.open("/T.TXT", FILE_WRITE) && !SD.exists("/"), "nothing opens before begin or after end");
}

int main(int argc, char **argv)
{
    const char *s = (argc > 1) ? argv[1] : "";
    if (strcmp(s, "unmounted") == 0) {
        t_unmounted();
        return failures ? 1 : 0;
    }
    if (!SD.begin()) {
        printf("FAIL  the RAM disk did not format and mount\n");
        return 1;
    }
    if      (strcmp(s, "write_read") == 0)    { t_write_read(); }
    else if (strcmp(s, "append") == 0)        { t_append(); }
    else if (strcmp(s, "seek_peek") == 0)     { t_seek_peek(); }
    else if (strcmp(s, "remove_exists") == 0) { t_remove_exists(); }
    else if (strcmp(s, "dirs") == 0)          { t_dirs(); }
    else if (strcmp(s, "root") == 0)          { t_root(); }
    else {
        printf("unknown scenario \"%s\"\n", s);
        return 2;
    }
    check(SD.end(), "end unmounts");
    return failures ? 1 : 0;
}
