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
| `SD_FileReceive` | SD 卡：PC 经 RS232 发来的文件（YMODEM） |

KNX 和外部 SDRAM 有各自的库 `OpenPLC_KNX` 和 `OpenPLC_SDRAM`，例程也在各自的库里。

## 模拟量端口要用内部基准

板子没有外部电压基准。`openplcEnableVref()` 打开芯片内部的 2.5 V 基准；
模拟量和温度的例程都先调用它。不开的话，所有模拟量读数都没有意义。

## 用 mV、mA 读写模拟量，并套用这块板的校准值

| 函数 | 给什么 |
|---|---|
| `openplcReadAI1_mV()` | AI1 的电压，mV（0–10 V 输入） |
| `openplcReadAI2_mA()` | AI2 的电流，mA（0–20 mA 输入） |
| `openplcWriteAO_mA(channel, mA)` | 把 AO1（`channel` 取 1）或 AO2（`channel` 取 2）设成一个电流，单位 mA，超出 0–20 mA 截到边界 |
| `openplcCalibrationStatus()` | 这块板的校准值是否有效：`CALIB_OK`，或 `CALIB_BLANK` / `CALIB_CORRUPT` / `CALIB_OTHER_BOARD` |

每块板都在产线工装上校准过，这几个函数会套用那份修正值。没有有效校准值的板子退回标称换算，
并在诊断串口打一行说明。它们自己打开内部基准，并把 ADC、DAC 的分辨率留在 12 位。
`analogRead()` / `analogWrite()` 不变，仍是原始值。

## 上电、复位、掉电时输出的状态

在你的 sketch 第一次写某个输出之前，板子让所有输出保持为 0。

| 输出 | 上电、复位后 bootloader 跑起来之前 | bootloader 运行时，以及 sketch 第一次写之前 | 掉电 |
|---|---|---|---|
| 继电器 RY1–RY6 | 断开 | 断开 | 断开 |
| 数字输出 DO1–DO8 | 断开 | 断开（bootloader 主动拉低） | 断开 |
| 模拟输出 AO1/AO2 | **不确定**，几毫秒 | 0 mA（bootloader 把输入拉低） | **不确定**，几毫秒 |

两个「不确定」的窗口还没实测，之后用示波器量。

3.3 V 电源掉到 2.7 V 以下时，芯片自己复位（欠压复位），输出回到上面的状态。
2.7 V 这个门限在出厂时设好；没设好的话，开机日志里会提示。

## 看门狗和报警输出由你自己实现

IEC 61131-2 要求 PLC 监视用户程序（看门狗），固定安装时还要有报警输出。板卡包不替你打开这两样：
什么算故障、用哪一路输出报警，由你决定。

- **看门狗**：用板卡包自带的 `IWatchdog` 库，在 `setup()` 里 `IWatchdog.begin(超时微秒数)`，
  在 `loop()` 里 `IWatchdog.reload()`。程序卡在某处不再喂狗，板子就会复位。
- **上次是不是看门狗复位的**：在 `setup()` 开头调 `openplcResetCause()`，返回
  `OPENPLC_RESET_WATCHDOG`、`OPENPLC_RESET_POWER_ON`、`OPENPLC_RESET_PIN`、
  `OPENPLC_RESET_SOFTWARE`、`OPENPLC_RESET_BROWNOUT` 或 `OPENPLC_RESET_UNKNOWN`。
  不要用 `IWatchdog.isReset()`：它读的那个标志 bootloader 已经清掉了，在本板上永远是 false。
- **每次都卡死的程序会每次都复位**，它控制的输出也跟着反复断开、接通。反复被看门狗复位之后怎么办，
  在你的 sketch 里决定。
- **报警输出**：选一路继电器或数字输出，设备正常时让它吸合，要报警时断开。这样掉电、复位、
  卡死时它没人驱动、自己断开，报警自然就发出去了。
