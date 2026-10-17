# OpenPLC_KNX

English: [README.md](README.md)

OpenPLC 板上的 KNX：组对象和 DPT 编码来自 [thelsing/knx](https://github.com/thelsing/knx) 协议栈，
走 KNXnet/IP（以太网）和 KNX TP 总线（端子 C03 / C04，STKNX 收发器）。

## 角色

sketch 开头写一行选协议栈的 mask，写法同 `OPENPLC_APP_VERSION`：

```cpp
OPENPLC_KNX_ROLE(TP_DEVICE);   // 或 OPENPLC_KNX_ROLE(IP_ROUTER);
```

| 角色 | 写哪一行 | Mask | 走哪条线 |
|---|---|---|---|
| KNX TP 设备 | `OPENPLC_KNX_ROLE(TP_DEVICE);` | `0x07B0` | TP；有组对象，由 ETS 关联组地址 |
| KNX IP 路由器 | `OPENPLC_KNX_ROLE(IP_ROUTER);` | `0x091A` | 在 KNXnet/IP 和 TP 之间转发，没有组对象 |

这一行要写在 `.ino` 里、单独一行。用了这个库却没写，编译报错，报错里给出要加的那一行。Tools 菜单里没有 KNX 角色。

两种都是 ETS 认识的 mask，ETS 都能给板子编程。
角色不变时，ETS 下载的内容在重新上传 sketch 后仍然保留。换了角色，或者板子是用更早的板卡包编程的，
开机时会丢弃这些数据（日志 `ETS has to reprogram PA and application!`），按未编程运行，直到 ETS 重新下载。
先从 [OpenPLC_ETS_Prod](https://github.com/haliboteda/OpenPLC_ETS_Prod) 下载 ETS 产品文件 `OpenPLC_TP.knxprod` 导入 ETS。

| 例程 | 角色 | 演示什么 |
|---|---|---|
| `KNX_Switch` | TP 设备 | 6 路继电器当开关执行器，由 ETS 编程 |
| `KNX_Inputs` | TP 设备 | 8 路 DI 当二进制输入，由 ETS 编程 |
| `KNX_Temperature` | TP 设备 | 收传感器的温度，过阈值（从串口监视器设，默认 27 °C）时开关另一台设备和 DO2，由 ETS 编程 |
| `KNX_Analog` | TP 设备 | AI1 / AI2 按 2 字节浮点发出去，总线写来的值设 AO1 / AO2 的电流，由 ETS 编程 |
| `KNX_BusMonitor` | TP 设备 | 打印总线上每一帧；串口监视器里输入的组写发到总线上。不用 ETS |
| `KNX_IPRouter` | IP 路由器 | 在 TP 和 KNXnet/IP 之间转发报文 |

例程用 `Serial` 打印：在板子 USB 口上的串口监视器里看。`Serial` 即 USB 口 1（也可写 `SerialUSB`）。例程不等电脑：开头那几行说明（KNX 地址）在串口监视器每次连上时打印，复位后重开串口监视器就能看到。

| 对象 | 端口 |
|---|---|
| `Serial` / `SerialUSB` | USB 口 1（板上的 USB 接口） |
| `RS232` | 端子 C05 / C06，真 RS232 电平；`RS232.begin()` 时才打开 |
| `RS485` | 端子 C10（A）/ C11（B） |

## `KNX` 对象

sketch 只用一个对象 `KNX`：

| 方法 | 做什么 |
|---|---|
| `setup(orderNumber = "OPENPLC-TP", manufacturerId = 0x00FA)` | 在 `setup()` 里调一次：板上引脚、编程键和指示灯，读入 ETS 下载的数据 |
| `start()` | 启动协议栈；设好组对象回调之后调 |
| `loop()` | 每次 `loop()` 都调 |
| `progMode()` | 是否在 ETS 编程模式 |
| `configured()` | ETS 是否已下载过应用程序 |
| `individualAddress()` | 本板的物理地址 |
| `getGroupObject(n)` | ETS 产品里第 `n` 号组对象，返回 `GroupObject&`（只有 TP 设备有） |
| `onBusFrame(cb)` | TP 总线上每一帧校验对的报文都交给 `cb`，不论发给谁（`KNX_BusMonitor`） |
| `sendGroupWrite(ga, value)` / `sendGroupWrite(ga, data, len)` | 不经 ETS 直接往 TP 总线发 GroupValue.Write：0..63 的小值，或 1..14 个数据字节 |
| `onBusSendDone(cb)` | 每次 `sendGroupWrite` 发完调 `cb(ok)`，`ok` = 收到应答 |

组对象编号（`KNX_OBJ_RELAY1_SWITCH` …）和 `OPENPLC_KNX_ORDER_NUMBER` 来自 `src/openplc_knx_objects.h`，`OpenPLC_KNX.h` 已经包含它。它由 `OpenPLC_ETS_Prod/tools/gen_product.py` 从 `knx_objects.json` 生成：要改就改 JSON，不要改头文件。

## TP 总线

STKNX 是裸 TP1 收发器，不是 TP-UART，位要库自己产生。TIM12 通道 1 在 `KNX_TX`（PB14）上把每个逻辑 0
发成 104 µs 位周期里的一个 35 µs 脉冲，TIM1 通道 3 给 `KNX_RX`（PA10）上的每个脉冲打时间戳。
字节、帧、帧尾后 15 个位时间的应答、重发和冲突处理全在这两个定时器中断里做，
所以 `loop()` 慢了也不会漏掉应答。

**用了这个库，TIM1 和 TIM12 就归它。** 同一个 sketch 里不要再拿它们做 PWM 或别的。

`KNX.setup()` 第一件事就是把 PB14 拉低，不论哪个角色：PB14 悬空时收发器会从总线抽电流。

## 例程依赖的板上事实

| 信号 | 引脚 | 说明 |
|---|---|---|
| 编程键 | PG9 | 按下为高（板上 10k 下拉）。和 BOOT0 是同一根线，库只读它。sketch 运行中按一下切换编程模式；**不要按着它上电或复位**，那会进 bootloader 的上传模式 |
| 编程模式指示 | PE2 | 系统指示灯：编程模式下常亮，系统日志同时记一行 `[sys][KNX] programming mode on` / `off`。用了这个库，系统灯归它用。（原理图上的 `KNX_Prog_LED` 即 PG11 没有装 LED） |
| 总线有电 | PH12 | 总线给收发器供上电时为高 |
| `KNX_OK` | PD7 | 这块板上不论总线怎样都读到低，不要用它判断总线 |

ETS 让板子重启时（比如下载过程中），库在复位之前往系统日志写一行，日志里就看得出是谁让它重启的：`[sys][KNX] restart requested by ETS (basic)` 或 `[sys][KNX] restart requested by ETS (master reset, erase code N)`。系统日志复位后还在，电脑打开 USB 口 1（`Serial`）时吐出。

## KNX 协议栈

`src/knx/` 是 thelsing/knx 的副本，去掉了 RF 和只给编辑器用的 `#pragma region` 折叠标记。ETS 数据存在 flash 扇区 6 开头的 16 KiB（`KNX_FLASH_SIZE`，板卡包对所有角色统一设定）。协议栈自己的调试输出默认关，编译时加 `-DKNX_DEBUG` 才以 `[sys][KNX]` 行写进系统日志。
