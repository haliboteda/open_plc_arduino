# OpenPLC_Ports

English: [README.md](README.md)

OpenPLC 板子每个端口一个例程。从 **文件 ▸ 示例 ▸ OpenPLC_Ports** 打开一个，
上传，然后打开串口监视器。

## 每个例程都是同一个样子

- 开头的注释说四件事：**做什么**、**接什么**、**应该看到什么**，以及
  **串口监视器**怎么设（板子的 USB 口，115200 波特率）。
- 第一行代码是 `OPENPLC_APP_VERSION(1, 0, 0);` —— 每个 sketch 都要有这一行。
- 例程如果要用库管理器里的库，开头注释的第一行会写明是哪个。
- 代码在端口允许的范围内尽量短。没有东西判断过不过；你把看到的和开头注释写的对照。

## 端口

| 例程 | 端口 |
|---|---|
| `DI_Inputs` | 数字输入 DI1–DI8 |
| `DO_Outputs` | 数字输出 DO1–DO8 |
| `Relays` | 继电器 RY1–RY6 |
| `SystemLED` | 系统指示灯 |
| `AI_Inputs` | 模拟输入 AI1（电压）和 AI2（电流） |
| `AO_Outputs` | 模拟输出 AO1–AO2（电流） |
| `BoardTemperature` | 板上的两个温度传感器 |
| `RS232_Echo` | RS232 |
| `RS485_Echo` | RS485 |
| `USB_Serial` | USB 串口 |
| `Ethernet_IP` | 以太网 |
| `CAN_Counter` | CAN |
| `SD_ReadWrite` | SD 卡 |

KNX 和外部 SDRAM 有各自的库 `OpenPLC_KNX` 和 `OpenPLC_SDRAM`，例程也在各自的库里。

## 模拟量端口要用内部基准

板子没有外部电压基准。`openplcEnableVref()` 打开芯片内部的 2.5 V 基准；
模拟量和温度的例程都先调用它。不开的话，所有模拟量读数都没有意义。
