# OpenPLC_SD

中文：[README.zh-CN.md](README.zh-CN.md)

The microSD card slot of the OpenPLC board, with a FAT32 / exFAT file system.
It ships with the board package; nothing to install from the Library Manager.

```cpp
#include <OpenPLC_SD.h>

SD.begin(SDMMC_CD_Pin);              // false: no card, or it cannot be read
File f = SD.open("LOG.TXT", FILE_WRITE);
f.println("hello");
f.close();
```

| Call | Does |
|---|---|
| `SD.begin(SDMMC_CD_Pin)` / `SD.end()` | Mount / unmount the card. The board's card-detect pin reads low while a card is in |
| `SD.cardPresent()` | Whether the detect pin says a card is in |
| `SD.open(path, FILE_READ)` | Open for reading. A directory opens too; walk it with `openNextFile()` |
| `SD.open(path, FILE_WRITE)` | Create if missing, write at the end |
| `SD.exists` / `SD.remove` / `SD.mkdir` / `SD.rmdir` | Path operations |
| `File`: `read` `write` `print` `peek` `available` `seek` `position` `size` `flush` `close` `name` `isDirectory` `openNextFile` `rewindDirectory` | As in Arduino's SD library |

A `File` is a handle: copies share one open file, so close it once. Paths are
limited to 255 characters.

Examples: **File ▸ Examples ▸ OpenPLC_Ports ▸ SD_ReadWrite** and
**SD_FileReceive**.

## Licence

The interface (`OpenPLC_SD.h`, `OpenPLC_SD.cpp`, `openplc_sd_mount.cpp`) is
written for this board package. The SD driver files (`bsp_sd.*`, `Sd2Card.*`,
`SdFatFs.*`, `ffconf*`) come from STMicroelectronics' STM32SD 1.5.0 and the
file system from the bundled FatFs library, all under BSD-3-Clause terms kept in
their headers and in `FatFs/LICENSE.md`. STM32SD's own `SD` / `File` classes are
GPL v3 and are **not** included, so sketches using this library carry no GPL
obligations.

Do not include `STM32SD.h` in the same sketch: both define `SD` and `File`.

If FatFs was installed from the Library Manager earlier, the IDE builds with
that copy instead of the bundled one (it prefers the sketchbook; the compile
output says "Multiple libraries were found for ff.h"). Remove it from
`Documents/Arduino/libraries` to use the bundled FatFs. A previously installed
STM32SD is not used unless a sketch includes `STM32SD.h`.
