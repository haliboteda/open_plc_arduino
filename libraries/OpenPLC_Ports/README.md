# OpenPLC_Ports

中文：[README.zh-CN.md](README.zh-CN.md)

One example per port of the OpenPLC board. Open one from
**File ▸ Examples ▸ OpenPLC_Ports**, upload it, and open the Serial Monitor.

## Every example follows the same shape

- The header comment says four things: **what it does**, **what to connect**,
  **what you should see**, and how to set up the **Serial Monitor** (the
  board's USB port, 115200 baud).
- The first line of code is `OPENPLC_APP_VERSION(1, 0, 0);` — every sketch
  needs one.
- If the example needs a library from the Library Manager, the header's first
  line says which.
- The code is as short as the port allows. Nothing checks pass or fail; you
  compare what you see with what the header says.

## Ports

| Example | Port |
|---|---|
| `DI_Inputs` | Digital inputs DI1–DI8 |
| `DO_Outputs` | Digital outputs DO1–DO8 |
| `Relays` | Relays RY1–RY6 |
| `SystemLED` | The system LED |
| `AI_Inputs` | Analog inputs AI1 (voltage) and AI2 (current) |
| `AO_Outputs` | Analog outputs AO1–AO2 (current) |
| `BoardTemperature` | The two on-board temperature sensors |
| `RS232_Echo` | RS232 |
| `RS485_Echo` | RS485 |
| `USB_Serial` | USB serial |
| `Ethernet_IP` | Ethernet |
| `CAN_Counter` | CAN |
| `SD_ReadWrite` | SD card |

KNX and the external SDRAM have their own libraries, `OpenPLC_KNX` and
`OpenPLC_SDRAM`, with their own examples.

## Analog ports need the internal reference

The board has no external voltage reference. `openplcEnableVref()` turns on the
chip's internal 2.5 V reference; the analog and temperature examples call it
first. Without it every analog reading is meaningless.

## Analog values in mV and mA, with this board's calibration

| Function | What it gives |
|---|---|
| `openplcReadAI1_mV()` | AI1 voltage in mV (0–10 V input) |
| `openplcReadAI2_mA()` | AI2 current in mA (0–20 mA input) |
| `openplcWriteAO_mA(channel, mA)` | sets AO1 (`channel` 1) or AO2 (`channel` 2) to a current in mA, clamped to 0–20 mA |
| `openplcCalibrationStatus()` | whether this board's calibration is valid: `CALIB_OK`, or `CALIB_BLANK` / `CALIB_CORRUPT` / `CALIB_OTHER_BOARD` |

Each board is calibrated on the production fixture; these functions apply that
correction. A board without valid calibration falls back to the nominal
conversion and prints one line saying so on the diagnostic serial port. They
turn on the internal reference themselves and leave the ADC and DAC at 12-bit
resolution. `analogRead()` / `analogWrite()` are unchanged and stay raw.

## Outputs at power-up, reset and power loss

Until your sketch first writes an output, the board keeps every output at 0.

| Output | Power-up and reset, before the bootloader runs | Bootloader, and your sketch until its first write | Power loss |
|---|---|---|---|
| Relays RY1-RY6 | off | off | off |
| Digital outputs DO1-DO8 | off | off (the bootloader drives them low) | off |
| Analog outputs AO1/AO2 | **not defined** for a few milliseconds | 0 mA (the bootloader drives the input low) | **not defined** for a few milliseconds |

The two "not defined" windows have not been measured yet; they will be checked
with an oscilloscope.

When the 3.3 V supply falls below 2.7 V the chip resets itself (brown-out
reset), so the outputs return to the states above. The 2.7 V level is set once
at production; if it is not, the boot log says so.

## Watchdog and alarm output are yours to build

IEC 61131-2 asks a PLC to watch the user program (a watchdog) and, when
permanently installed, to drive an alarm output. The board package turns neither
on for you: what counts as a fault and which output raises the alarm are your
decisions.

- **Watchdog**: use the `IWatchdog` library that ships with the board package,
  `IWatchdog.begin(timeout_us)` in `setup()` and `IWatchdog.reload()` in
  `loop()`. If your program then hangs in a loop, the board resets.
- **Was the last reset a watchdog?** Call `openplcResetCause()` at the top of
  `setup()`. It returns `OPENPLC_RESET_WATCHDOG`, `OPENPLC_RESET_POWER_ON`,
  `OPENPLC_RESET_PIN`, `OPENPLC_RESET_SOFTWARE`, `OPENPLC_RESET_BROWNOUT` or
  `OPENPLC_RESET_UNKNOWN`. Do not use `IWatchdog.isReset()`: the bootloader has
  already cleared the flag it reads, so it is always false on this board.
- **A program that hangs every time resets every time**, and the outputs it
  drives go off and on again with it. Decide in your sketch what to do after
  repeated watchdog resets.
- **Alarm output**: pick a relay or a digital output and drive it on while the
  machine is healthy; switch it off to raise the alarm. A power loss, a reset or
  a hang then raises the alarm by themselves, because the output drops when
  nothing drives it.
