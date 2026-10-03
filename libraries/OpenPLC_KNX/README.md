# OpenPLC_KNX

中文：[README.zh-CN.md](README.zh-CN.md)

KNX for the OpenPLC board: group objects and DPT encoding from the
[thelsing/knx](https://github.com/thelsing/knx) stack, carried over KNXnet/IP
(Ethernet) and over the KNX TP bus (terminals C03 / C04, STKNX transceiver).

## Roles

The **Tools → KNX Role** menu picks the stack's mask version:

| Role | Mask | Transports |
|---|---|---|
| IP+TP device (default) | `0x5780` | KNXnet/IP and TP; outgoing telegrams go on both |
| KNXnet/IP device | `0x57B0` | IP only |
| KNX TP device | `0x07B0` | TP only |
| IP/TP coupler | `0x091A` | routes between IP and TP, no local group objects |

## The TP bus

The STKNX is a bare TP1 transceiver, not a TP-UART: the library makes the bits
itself. TIM12 channel 1 puts each logical 0 on `KNX_TX` (PB14) as a 35 µs pulse
in a 104 µs bit period, and TIM1 channel 3 timestamps every pulse on `KNX_RX`
(PA10). Characters, frames, the acknowledge 15 bit times after a frame,
repetitions and collision handling all run inside those two timer interrupts,
so a slow `loop()` does not make the board miss an acknowledge.

**Using this library reserves TIM1 and TIM12.** Do not use them for PWM or
anything else in the same sketch.

`KNXHelper.setup()` drives PB14 low first thing, whatever the role: a floating
PB14 makes the transceiver draw current from the bus.

## Board facts the examples rely on

| Signal | Pin | Note |
|---|---|---|
| Programming button | PG9 | Pressed = high (external 10k pull-down). Same net as BOOT0 — the library only reads it |
| `KNX_Prog_LED` | PG11 | **No LED is fitted on this line.** The library still drives it; read programming mode with `KNXHelper.progMode()` |
| Bus voltage present | PH12 | High when the bus powers the transceiver |
| `KNX_OK` | PD7 | Reads low on this board whatever the bus does; do not use it to detect the bus |
| Relays 1 / 2 | PI8 / PI10 | High = coil energised (Lower Deck, terminals B01–B04) |

The examples that print to `Serial_Test` (RS232, terminals C05 / C06) switch the
RS232 transceiver on with `RS232_EN_Pin` first; it is off by default.
