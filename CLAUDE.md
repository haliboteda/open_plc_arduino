# 开工入口 —— Arduino 板卡包（app 侧）

**这份文件是给 AI 会话看的。**

这个仓库是给 Schaeffer AG OpenPLC 板子（STM32H743）定制的 **Arduino 板卡包**：用户的 PLC 程序用它编译。它同时持有 `cores/arduino/main.cpp`、变体头文件、引脚与外设映射，以及 `OpenPLC_IAP` / `OpenPLC_Net` / `OpenPLC_SDRAM` 三个库。

## ⚠️ 这是要分发给其他工程师的基础设施

改这里**不是本地小修小补**。任何改动都会跟着板卡包发出去。

## ⚠️ 共享文档不在这个仓库里

产品的需求、架构、硬件事实、设计决策、协作规矩，**全部在 `open_plc_cube_ide/docs/` 下**，那里是唯一出处。这份文件不抄，只指路。

```
git clone git@github.com:haliboteda/open_plc_cube_ide.git
```

然后读它根目录的 `CLAUDE.md` —— **换机器要 clone 什么、装什么、配什么，那一份写全了**。

## ⚠️ 改动方向是单向的：live → repo

| | 是什么 | 状态 |
|---|---|---|
| `$CORE_LIVE` | Arduino IDE **真正加载**的那份（板卡包安装目录，在 `Arduino15/packages/...` 下） | **不在版本控制下** |
| `$CORE_REPO` | 就是本仓库 | git |

**流程固定：在 `$CORE_LIVE` 里改 → 在那里编译、烧板、验证 → 只有验证通过的才拷进本仓库提交。**

- 反过来做没有意义 —— IDE 根本不看本仓库，改这边不生效
- ⚠️ **验证通过后忘了拷回来，那段代码就只存在于一台机器上**，重装一次 IDE 就没了
- 核对两边是否同步：`IAPTranfer_Tool/TestTool/tools/check-core-sync.ps1`（用例 **P3**，也是 `selfcheck.ps1` 的 A9）

## ⚠️ 有些代码在别的仓库里有一份镜像

没有共享构建系统，所以下面这些东西**在多个仓库里各有一份拷贝，只能靠注释交叉引用约束，机制上无法强制同步**。改一处必须改另一处，否则会**静默分叉** —— 不会编译报错，只会在运行时表现成别的症状。

清单和 RTC 备份寄存器的分配表在 `open_plc_cube_ide/docs/design/ARCHITECTURE.md`，**认领任何一个备份寄存器之前先看那张表**（已经撞过一次车，后果是 app 每次经过 bootloader 之后重复发放同一批 nonce）。

自动比对：`IAPTranfer_Tool/TestTool/tools/check-mirror-sync.ps1`（用例 **P2**，`selfcheck.ps1` 的 A8）。

## 设计不能限制用户的 app

**不论用户在 app 里怎么用这颗芯片，设计都必须依然正确。**

不能依赖 app 恰好关掉了某个功能（缓存、MPU、某块 RAM、某个外设）。正确性靠**显式动作**保证，不靠"当前配置恰好如此"。发现设计可能和用户 app 的自由度冲突时，**必须主动提出来问**。

> 已经吃过一次：DBP 位被前一步关掉导致以太网升级静默失败，而 CDC 路径只是恰好没踩到。

## 构建

```
arduino-cli compile --warnings all --config-file <arduino-cli.yaml> --fqbn <见 BUILD-AND-TEST.md> <sketch>
```

`--config-file` 和 `--warnings all` **都必须带**，理由在 `open_plc_cube_ide/docs/test/BUILD-AND-TEST.md`。IDE 自带的 `arduino-cli` 不在 PATH 上。

例程能否全部编过：`IAPTranfer_Tool/TestTool/host/examples_build/build.ps1`。

## 语言

代码注释、`#error` / `#warning` 文案、`README` 一律**英文**；本文件用中文。
