/*
 * OpenPLC_SD.h -- the microSD card slot and its FAT32 / exFAT file system.
 *
 * Shaped like Arduino's SD library (SD.begin, SD.open, File) so sketches read
 * the same, but written over FatFs here so it carries no GPL terms
 * (decision 84, $PROD/docs/tables/DECISIONS.md). Do not include it together
 * with STM32SD.h: both define SD and File.
 */
#ifndef OPENPLC_SD_H_
#define OPENPLC_SD_H_

#include <Arduino.h>
/* bsp_sd.h before ff.h: ff.h includes ffconf.h inside extern "C", and
 * ffconf.h includes bsp_sd.h, which pulls in C++ core headers. */
#ifndef OPENPLC_SD_HOST_TEST
#include "bsp_sd.h"
#endif
#include "ff.h"

#undef FILE_READ
#undef FILE_WRITE
#define FILE_READ  FA_READ
/* Creates the file if needed and writes at its end, as Arduino's SD does. */
#define FILE_WRITE (FA_READ | FA_WRITE | FA_OPEN_APPEND)

#define OPENPLC_SD_NO_DETECT 0xFFFFFFFFUL
#define OPENPLC_SD_PATH_MAX  256

class File : public Stream {
  public:
    File();

    size_t write(uint8_t b) override;
    size_t write(const uint8_t *buf, size_t len) override;
    using Print::write;
    int available() override;
    int read() override;
    int peek() override;
    void flush() override;

    int read(void *buf, size_t len);
    bool seek(uint32_t pos);
    uint32_t position();
    uint32_t size();
    void close();

    operator bool() const;
    /* The last part of the path, e.g. "LOG.TXT". */
    const char *name() const;
    bool isDirectory() const;
    /* For a directory: the next entry, or a closed File at the end. */
    File openNextFile(uint8_t mode = FILE_READ);
    void rewindDirectory();

  private:
    friend class SDClass;
    /* A File is a handle: copies share one open file, so close it once. */
    FIL *_file;
    DIR *_dir;
    char _path[OPENPLC_SD_PATH_MAX];
};

class SDClass {
  public:
    /* detect: the card-detect pin (SDMMC_CD_Pin on this board), read as
     * "card in" at level. False if no card is in or it cannot be mounted. */
    bool begin(uint32_t detect = OPENPLC_SD_NO_DETECT, uint32_t level = LOW);
    bool end();
    bool cardPresent();

    File open(const char *path, uint8_t mode = FILE_READ);
    bool exists(const char *path);
    bool remove(const char *path);
    bool mkdir(const char *path);
    bool rmdir(const char *path);

  private:
    uint32_t _detect = OPENPLC_SD_NO_DETECT;
    uint32_t _level = LOW;
    bool _mounted = false;
};

extern SDClass SD;

#endif /* OPENPLC_SD_H_ */
