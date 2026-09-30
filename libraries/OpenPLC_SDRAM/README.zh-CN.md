# OpenPLC_SDRAM

English: [README.md](README.md)

在应用程序里使用板子上 64 MB 的外部 SDRAM（AS4C32M16SB，挂在 FMC 总线上，
地址 `0xC0000000`）。

```cpp
#include <OpenPLC_SDRAM.h>

void setup() {
  if (!SDRAM.begin()) {
    // no usable SDRAM -- decide what that means for this application
  }
  uint16_t *samples = (uint16_t *)SDRAM.alloc(2 * 1024 * 1024);  // already zeroed
}
```

从 `SDRAM_Basic` 例程开始；`SDRAM_DataLogger` 展示这块内存存在的理由 ——
一个远大于 512 KB 内部 RAM 的历史缓冲区。

## API

| 调用 | 作用 |
|---|---|
| `SDRAM.begin()` | 给控制器和芯片上电，检查内存能正常应答。可以重复调用。**内存不可用时返回 `false`。** |
| `SDRAM.ready()` | `begin()` 是否已经成功 |
| `SDRAM.alloc(bytes, align = 8)` | 返回 `bytes` 字节**已清零**的内存，或 `nullptr` |
| `SDRAM.allocUninitialized(bytes, align = 8)` | 同上但不清零 —— 缓冲区里是之前留下的内容 |
| `SDRAM.available()` / `SDRAM.used()` | 剩余字节 / 已分配字节 |
| `OpenPLC_SDRAM_Class::CAPACITY` | 64 MiB |

## 要知道的三件事

**1. 没有 `free()`。** 分配只往前走。在 `setup()` 里一次要够，整个程序运行期间一直用着。
PLC 在这里不需要堆，堆只会带来碎片和一种新的出错方式。

**2. 清零不是免费的。** 在这块板子上实测：**91 MB/s**，1 MB 大约 11 ms，
整个 64 MB 大约 700 ms。放在 `setup()` 里没问题，放在 `loop()` 里不行。
如果一个大缓冲区马上就会被整个覆盖，`allocUninitialized()` 可以跳过清零 ——
名字难看是故意的，因为看起来像样的未初始化数据正是这个库要消除的 bug。

**3. SDRAM 断电不保存。** 它是工作内存，不是存储。
断电后还要留着的东西放 flash 或 SD 卡。

## 为什么不能把变量直接声明「在 SDRAM 里」

没有 `__attribute__((section(".sdram_bss")))` 这种办法把大数组直接放进 SDRAM。
最初的设计是这样的，后来放弃了，因为它给你四种出错的方式，其中三种不出声：

- 这个段必须是 `NOLOAD`，否则一个 1 MB 的数组会让固件镜像大 1 MB；
- 它必须在 `_sbss.._ebss` 之外，否则启动时的清零循环会在*控制器还不存在时*写 SDRAM，板子直接出错；
- 它不能有初始值，原因相同，只是换成 `.data` 的拷贝；
- 而且内存**不会清零** —— 数组里是上次断电时留下的东西，所以程序大多数时候看起来是对的。

只能从 `alloc()` 拿到的地址，不可能在控制器起来之前被碰到，因为 `begin()` 成功之前
`alloc()` 一直返回 `nullptr`。这些坑不是写进文档，而是直接不存在了。

## 如果 `begin()` 返回 false

读回检查失败：内存应答不对。这时 `alloc()` 返回 `nullptr`，而不是交出会悄悄丢数据的地址。
怎么处理由应用程序决定 —— 数据记录器可以改用 SRAM 里更短的历史继续跑，
而离开这块缓冲区就干不了活的程序应该说出来并停下。

bootloader 启动时做同样的检查，并在 RS232 端子上打印 `SDRAM staging buffer OK` 或
`** SDRAM SELF-TEST FAILED **`，这是区分板子问题和 sketch 问题最快的办法。
