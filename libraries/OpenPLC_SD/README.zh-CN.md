# OpenPLC_SD

English: [README.md](README.md)

OpenPLC 板子的 microSD 卡座，带 FAT32 / exFAT 文件系统。随板卡包提供，不用从库管理器装。

```cpp
#include <OpenPLC_SD.h>

SD.begin(SDMMC_CD_Pin);              // 返回 false：没插卡，或卡读不了
File f = SD.open("LOG.TXT", FILE_WRITE);
f.println("hello");
f.close();
```

| 调用 | 做什么 |
|---|---|
| `SD.begin(SDMMC_CD_Pin)` / `SD.end()` | 挂载 / 卸载卡。板子的卡检测脚插着卡时读到低 |
| `SD.cardPresent()` | 检测脚是否说插着卡 |
| `SD.open(path, FILE_READ)` | 只读打开。目录也能打开，用 `openNextFile()` 逐项列 |
| `SD.open(path, FILE_WRITE)` | 没有就创建，从末尾接着写 |
| `SD.exists` / `SD.remove` / `SD.mkdir` / `SD.rmdir` | 路径操作 |
| `File`：`read` `write` `print` `peek` `available` `seek` `position` `size` `flush` `close` `name` `isDirectory` `openNextFile` `rewindDirectory` | 和 Arduino 的 SD 库一样 |

`File` 是句柄：拷贝出来的几份共用一个打开的文件，只关一次。路径最长 255 个字符。

例程：**文件 ▸ 示例 ▸ OpenPLC_Ports ▸ SD_ReadWrite** 和 **SD_FileReceive**。

## 许可证

接口那层（`OpenPLC_SD.h`、`OpenPLC_SD.cpp`、`openplc_sd_mount.cpp`）是板卡包自己写的。SD 驱动文件（`bsp_sd.*`、`Sd2Card.*`、`SdFatFs.*`、`ffconf*`）取自意法半导体的 STM32SD 1.5.0，文件系统是随带的 FatFs，都按 BSD-3-Clause 发布，条款留在各文件头和 `FatFs/LICENSE.md` 里。STM32SD 自己的 `SD` / `File` 两个类是 GPL v3，**没有收进来**，所以用这个库的 sketch 没有 GPL 义务。

同一个 sketch 里不要再包含 `STM32SD.h`：两边都定义了 `SD` 和 `File`。

以前从库管理器装过 FatFs 的，IDE 会改用那一份，而不是板卡包自带的（它优先用 sketchbook 里的；编译输出会写 "Multiple libraries were found for ff.h"）。要用自带的，把它从 `文档/Arduino/libraries` 里删掉。以前装过的 STM32SD 不会被用到，除非 sketch 包含了 `STM32SD.h`。
