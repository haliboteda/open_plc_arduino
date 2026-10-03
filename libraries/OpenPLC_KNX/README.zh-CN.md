# OpenPLC_KNX

English: [README.md](README.md)

OpenPLC 板上的 KNX：组对象和 DPT 编码来自 [thelsing/knx](https://github.com/thelsing/knx) 协议栈，
走 KNXnet/IP（以太网）和 KNX TP 总线（端子 C03 / C04，STKNX 收发器）。

## 角色

**Tools → KNX Role** 菜单选协议栈的 mask：

| 角色 | Mask | 走哪条线 |
|---|---|---|
| IP+TP 设备（默认） | `0x5780` | KNXnet/IP 和 TP；发出去的报文两边都发 |
| KNXnet/IP 设备 | `0x57B0` | 只走 IP |
| KNX TP 设备 | `0x07B0` | 只走 TP |
| IP/TP 耦合器 | `0x091A` | 在 IP 和 TP 之间转发，没有本地组对象 |

## TP 总线

STKNX 是裸 TP1 收发器，不是 TP-UART，位要库自己产生。TIM12 通道 1 在 `KNX_TX`（PB14）上把每个逻辑 0
发成 104 µs 位周期里的一个 35 µs 脉冲，TIM1 通道 3 给 `KNX_RX`（PA10）上的每个脉冲打时间戳。
字节、帧、帧尾后 15 个位时间的应答、重发和冲突处理全在这两个定时器中断里做，
所以 `loop()` 慢了也不会漏掉应答。

**用了这个库，TIM1 和 TIM12 就归它。** 同一个 sketch 里不要再拿它们做 PWM 或别的。

`KNXHelper.setup()` 第一件事就是把 PB14 拉低，不论哪个角色：PB14 悬空时收发器会从总线抽电流。

## 例程依赖的板上事实

| 信号 | 引脚 | 说明 |
|---|---|---|
| 编程键 | PG9 | 按下为高（板上 10k 下拉）。和 BOOT0 是同一根线，库只读它 |
| `KNX_Prog_LED` | PG11 | **这根线上没有装 LED。** 库照样驱动它；进没进编程模式用 `KNXHelper.progMode()` 读 |
| 总线有电 | PH12 | 总线给收发器供上电时为高 |
| `KNX_OK` | PD7 | 这块板上不论总线怎样都读到低，不要用它判断总线 |
| 继电器 1 / 2 | PI8 / PI10 | 高 = 线圈通电（Lower Deck，端子 B01–B04） |

往 `Serial_Test`（RS232，端子 C05 / C06）打印的例程先用 `RS232_EN_Pin` 打开 RS232 收发器，它默认是关的。
