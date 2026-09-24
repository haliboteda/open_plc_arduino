# OpenPLC_Ports

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
