/*
 * OpenPLC_SD.cpp -- File and the path operations, over FatFs only. Mounting
 * the card lives in openplc_sd_mount.cpp, so the host test can mount a RAM
 * disk in its place.
 */
#include "OpenPLC_SD.h"

#include <string.h>

SDClass SD;

static bool is_root(const char *path)
{
    return path[0] == '\0' || (path[0] == '/' && path[1] == '\0');
}

File::File() : _file(nullptr), _dir(nullptr)
{
    _path[0] = '\0';
}

size_t File::write(uint8_t b)
{
    return write(&b, 1);
}

size_t File::write(const uint8_t *buf, size_t len)
{
    UINT n = 0;
    if (_file == nullptr || f_write(_file, buf, (UINT)len, &n) != FR_OK) {
        setWriteError();
        return 0;
    }
    return n;
}

int File::read(void *buf, size_t len)
{
    UINT n = 0;
    if (_file == nullptr || f_read(_file, buf, (UINT)len, &n) != FR_OK) {
        return -1;
    }
    return (int)n;
}

int File::read()
{
    uint8_t c;
    return (read(&c, 1) == 1) ? c : -1;
}

int File::peek()
{
    if (_file == nullptr) {
        return -1;
    }
    FSIZE_t at = f_tell(_file);
    int c = read();
    f_lseek(_file, at);
    return c;
}

int File::available()
{
    if (_file == nullptr) {
        return 0;
    }
    FSIZE_t left = f_size(_file) - f_tell(_file);
    return (left > 0x7FFF) ? 0x7FFF : (int)left;
}

void File::flush()
{
    if (_file != nullptr) {
        f_sync(_file);
    }
}

bool File::seek(uint32_t pos)
{
    return _file != nullptr && f_lseek(_file, pos) == FR_OK;
}

uint32_t File::position()
{
    return (_file != nullptr) ? (uint32_t)f_tell(_file) : 0;
}

uint32_t File::size()
{
    return (_file != nullptr) ? (uint32_t)f_size(_file) : 0;
}

void File::close()
{
    if (_file != nullptr) {
        f_close(_file);
        delete _file;
        _file = nullptr;
    }
    if (_dir != nullptr) {
        f_closedir(_dir);
        delete _dir;
        _dir = nullptr;
    }
}

File::operator bool() const
{
    return _file != nullptr || _dir != nullptr;
}

const char *File::name() const
{
    const char *slash = strrchr(_path, '/');
    return slash ? slash + 1 : _path;
}

bool File::isDirectory() const
{
    return _dir != nullptr;
}

File File::openNextFile(uint8_t mode)
{
    FILINFO info;
    if (_dir == nullptr || f_readdir(_dir, &info) != FR_OK || info.fname[0] == '\0') {
        return File();
    }
    char child[OPENPLC_SD_PATH_MAX];
    size_t len = strlen(_path);
    bool slash = len > 0 && _path[len - 1] == '/';
    if ((size_t)snprintf(child, sizeof(child), slash ? "%s%s" : "%s/%s", _path, info.fname) >= sizeof(child)) {
        return File();   // path too long for a handle
    }
    return SD.open(child, mode);
}

void File::rewindDirectory()
{
    if (_dir != nullptr) {
        f_readdir(_dir, nullptr);
    }
}

File SDClass::open(const char *path, uint8_t mode)
{
    File f;
    if (!_mounted || strlen(path) >= OPENPLC_SD_PATH_MAX) {
        return f;
    }
    FILINFO info;
    if (is_root(path) || (f_stat(path, &info) == FR_OK && (info.fattrib & AM_DIR))) {
        f._dir = new DIR;
        if (f_opendir(f._dir, is_root(path) ? "/" : path) != FR_OK) {
            delete f._dir;
            f._dir = nullptr;
            return f;
        }
    } else {
        f._file = new FIL;
        if (f_open(f._file, path, mode) != FR_OK) {
            delete f._file;
            f._file = nullptr;
            return f;
        }
    }
    strcpy(f._path, path);
    return f;
}

bool SDClass::exists(const char *path)
{
    FILINFO info;
    return _mounted && (is_root(path) || f_stat(path, &info) == FR_OK);
}

bool SDClass::remove(const char *path)
{
    return _mounted && f_unlink(path) == FR_OK;
}

bool SDClass::mkdir(const char *path)
{
    return _mounted && f_mkdir(path) == FR_OK;
}

bool SDClass::rmdir(const char *path)
{
    return _mounted && f_unlink(path) == FR_OK;   // FatFs removes empty directories only
}

bool SDClass::cardPresent()
{
    return _detect == OPENPLC_SD_NO_DETECT || digitalRead(_detect) == (int)_level;
}
