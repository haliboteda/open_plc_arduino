# 开工入口 —— Arduino 板卡包（app 侧）

**这份文件是给 AI 会话看的。**

这个仓库是给 Schaeffer AG OpenPLC 板子（STM32H743）定制的 **Arduino 板卡包**：用户的 PLC 程序用它编译。它同时持有 `cores/arduino/main.cpp`、变体头文件、引脚与外设映射，以及 `OpenPLC_IAP` / `OpenPLC_Net` / `OpenPLC_SDRAM` 三个库。

> 产品全貌：`<AI-Skills>/OpenPLC/docs/OVERVIEW.md`（本机位置见 `SKILLS_REPO`）。

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
- 核对两边是否同步：`IAPTranfer_Tool/TestCase/tools/check-core-sync.ps1`（用例 **P3**）

## ⚠️ 有些代码在别的仓库里有一份镜像

清单、后果、以及 RTC 备份寄存器的分配表，都在 `$PROD/docs/design/ARCHITECTURE.md`。**认领任何一个备份寄存器之前先看那张表**（已经撞过一次车）。

自动比对：`IAPTranfer_Tool/TestCase/tools/check-mirror-sync.ps1`（用例 **P2**）。

## 设计不能限制用户的 app

**不论用户在 app 里怎么用这颗芯片，设计都必须依然正确。** 这条同时约束 bootloader 和板卡包，所以它只有一个家：`$PROD/docs/design/CONSTRAINTS.md`。

## 构建

```
arduino-cli compile --warnings all --config-file <arduino-cli.yaml> --fqbn <见 $PROD/docs/test/BUILD-AND-TEST.md> <sketch>
```

`--config-file` 和 `--warnings all` **都必须带**，理由在 `$PROD/docs/test/BUILD-AND-TEST.md`。IDE 自带的 `arduino-cli` 不在 PATH 上。

例程能否全部编过：`IAPTranfer_Tool/TestCase/host/examples_build/build.ps1`。

## 语言

代码注释、`#error` / `#warning` 文案、`README` 一律**英文**；本文件用中文。
