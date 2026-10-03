# 开工入口 —— Arduino 板卡包（app 侧）

**这份文件是给 AI 会话看的。**

这个仓库是给 Schaeffer AG OpenPLC 板子（STM32H743）定制的 **Arduino 板卡包**：用户的 PLC 程序用它编译。它同时持有 `cores/arduino/main.cpp`、变体头文件、引脚与外设映射，以及 `OpenPLC_IAP` / `OpenPLC_Net` / `OpenPLC_SDRAM` 三个库。

> **产品文档在 `OpenPLC_Docs`**（`$PROD`）—— 全部文档和待决的问题，入口它的 `README.md`（本机位置见 `DOCS_REPO`）。

## ⚠️ 这是要分发给其他工程师的基础设施

改这里**不是本地小修小补**。任何改动都会跟着板卡包发出去。

## ⚠️ 共享文档不在这个仓库里

出处在 `open_plc_cube_ide`：

```
git clone git@github.com:haliboteda/open_plc_cube_ide.git
```

读它根目录的 `CLAUDE.md`。

## ⚠️ 改动方向是单向的：live → repo

| | 是什么 | 状态 |
|---|---|---|
| `$CORE_LIVE` | Arduino IDE **真正加载**的那份（板卡包安装目录，在 `Arduino15/packages/...` 下） | **不在版本控制下** |
| `$CORE_REPO` | 就是本仓库 | git |

**流程固定：在 `$CORE_LIVE` 里改 → 在那里编译、烧板、验证 → 只有验证通过的才拷进本仓库提交。**

- 反过来做没有意义 —— IDE 根本不看本仓库，改这边不生效
- ⚠️ **验证通过后忘了拷回来，那段代码就只存在于一台机器上**，重装一次 IDE 就没了
- 核对两边是否同步：`tests/check_core_sync.py`（用例 **P3**）

## ⚠️ 有些代码在别的仓库里有一份镜像

清单、后果、以及 RTC 备份寄存器的分配表，都在 `$PROD/docs/repo/ARCHITECTURE.md`。**认领任何一个备份寄存器之前先看那张表**（已经撞过一次车）。

自动比对：`$TEST/tools/check_mirror_sync.py`（用例 **P2**）。

## 设计不能限制用户的 app

**设计不能限制用户 app 怎么用这颗芯片** —— 这条同时约束 bootloader 和板卡包，原文在 `$PROD/docs/modules/M3/CONSTRAINTS.md`。

## 构建

```
arduino-cli compile --warnings all --config-file <arduino-cli.yaml> --fqbn <见 $PROD/docs/build/BUILD-AND-TEST.md> <sketch>
```

`--config-file` 和 `--warnings all` **都必须带**，理由在 `$PROD/docs/build/BUILD-AND-TEST.md`。IDE 自带的 `arduino-cli` 不在 PATH 上。

例程能否全部编过：`tests/examples_build/build.py`（用例 **P5**），见下一节。

## 测试怎么跑

本仓自己的测试都在 `tests/`，只测板卡包，不需要别的仓（决策 78）。**不进发布包**：`.gitattributes` 里 `tests/ export-ignore`，GitHub 按 tag 打的发布包不含它。

```
python tests/selfcheck.py          # P3、P19、P4、P15、T2-21、T3-07、T3-08，几分钟
python tests/selfcheck.py --full   # 再加 P5（全部例程能编过），约 45 分钟
```

| 用例 | 测什么 | 在哪 |
|---|---|---|
| P3 | IDE 装着的那份（`$CORE_LIVE`）和本仓一致 | `tests/check_core_sync.py` |
| P19 | 板卡包里没有代码写 flash 扇区 15 | `tests/check_no_sector15_writes.py` |
| P4 | 变体头断言（FMC 保留脚、UART 走线） | `tests/variant_check/` |
| P15 | app 起始地址保持 1024 对齐 | `tests/vector_alignment/` |
| P5 | 全部例程能编过 | `tests/examples_build/` |
| T2-21 | 当前生效的根撤不掉自己（编真的 `owner_root_ro.c`） | `tests/owner_revoke/`，CMake/CTest |
| T3-07 | 带单位 AI / AO 套用校准值，无效时退回标称且只打一次日志（编真的 `openplc_calib.c`、`openplc_analog.c`） | `tests/calibrated_analog/`，CMake/CTest |
| T3-08 | bootloader 发布的复位原因传到 sketch，`openplcResetCause()` 译对（编 core 那份 `IAP_boot_handoff.c` 和 `openplc_reset.c`） | `tests/reset_cause/`，CMake/CTest |

**不用本机配置文件**，路径全从环境变量来，缺什么就报 SKIP 并点名：`ARDUINO_CLI`（IDE 自带的那个不在 PATH 上）、`ARDUINO_CLI_CONFIG`、`CMAKE`（不在 PATH 上时）；`ARDUINO15` / `CORE_LIVE` 一般不用设，按平台默认位置找。T2-21、T3-07 的本机编译器写在 `tests/CMakeUserPresets.json`（gitignored，preset 名 `local`，继承 `tests/CMakePresets.json` 的 `host`）。

⚠️ **P4、P5、P15 编的是 `$CORE_LIVE`，不是本仓。** arduino-cli 不肯把本仓当板卡包加载：`platform.txt` 的 `version=0.1.0rc0` 不是合法版本号，放进 sketchbook 的 `hardware/` 时报 `invalid patch version separator 'r'`（2026-10-02 实测）。P3 保证两份一致，所以先过 P3 再看这三项。

## 语言

代码注释、`#error` / `#warning` 文案、`README` 一律**英文**；本文件用中文。
