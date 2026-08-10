/*
 *******************************************************************************
 * Copyright (c) 2020, STMicroelectronics
 * All rights reserved.
 *
 * This software component is licensed by ST under BSD 3-Clause license,
 * the "License"; You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                        opensource.org/licenses/BSD-3-Clause
 *
 *******************************************************************************
 */
#pragma once

/*----------------------------------------------------------------------------
 *        STM32 pins number
 *----------------------------------------------------------------------------*/
#define PA0                     PIN_A0
#define PA1                     PIN_A1
#define PA2                     PIN_A2
#define PA3                     PIN_A3
#define PA4                     PIN_A4
#define PA5                     PIN_A5
#define PA6                     PIN_A6
#define PA7                     PIN_A7
#define PA8                     8
#define PA9                     9
#define PA10                    10
#define PA11                    11
#define PA12                    12
#define PA13                    13
#define PA14                    14
#define PA15                    15
#define PB0                     PIN_A8
#define PB1                     PIN_A9
#define PB2                     18
#define PB3                     19
#define PB4                     20
#define PB5                     21
#define PB6                     22
#define PB7                     23
#define PB8                     24
#define PB9                     25
#define PB10                    26
#define PB11                    27
#define PB12                    28
#define PB13                    29
#define PB14                    30
#define PB15                    31
#define PC0                     PIN_A10
#define PC1                     PIN_A11
#define PC4                     PIN_A12
#define PC5                     PIN_A13
#define PC6                     36
#define PC7                     37
#define PC8                     38
#define PC9                     39
#define PC10                    40
#define PC11                    41
#define PC12                    42
#define PC13                    43
#define PC14                    44
#define PC15                    45
#define PD0                     46
#define PD1                     47
#define PD2                     48
#define PD3                     49
#define PD4                     50
#define PD5                     51
#define PD6                     52
#define PD7                     53
#define PD8                     54
#define PD9                     55
#define PD10                    56
#define PD11                    57
#define PD12                    58
#define PD13                    59
#define PD14                    60
#define PD15                    61
#define PE0                     62
#define PE1                     63
#define PE2                     64
#define PE3                     65
#define PE4                     66
#define PE5                     67
#define PE6                     68
#define PE7                     69
#define PE8                     70
#define PE9                     71
#define PE10                    72
#define PE11                    73
#define PE12                    74
#define PE13                    75
#define PE14                    76
#define PE15                    77
#define PF0                     78
#define PF1                     79
#define PF2                     80
#define PF3                     PIN_A14
#define PF4                     PIN_A15
#define PF5                     PIN_A16
#define PF6                     PIN_A17
#define PF7                     PIN_A18
#define PF8                     PIN_A19
#define PF9                     PIN_A20
#define PF10                    PIN_A21
#define PF11                    PIN_A22
#define PF12                    PIN_A23
#define PF13                    PIN_A24
#define PF14                    PIN_A25
#define PF15                    93
#define PG0                     94
#define PG1                     95
#define PG2                     96
#define PG3                     97
#define PG4                     98
#define PG5                     99
#define PG6                     100
#define PG7                     101
#define PG8                     102
#define PG9                     103
#define PG10                    104
#define PG11                    105
#define PG12                    106
#define PG13                    107
#define PG14                    108
#define PG15                    109
#define PH0                     110
#define PH1                     111
#define PH2                     PIN_A26
#define PH3                     PIN_A27
#define PH4                     PIN_A28
#define PH5                     PIN_A29
#define PH6                     116
#define PH7                     117
#define PH8                     118
#define PH9                     119
#define PH10                    120
#define PH11                    121
#define PH12                    122
#define PH13                    123
#define PH14                    124
#define PH15                    125
#define PI0                     126
#define PI1                     127
#define PI2                     128
#define PI3                     129
#define PI4                     130
#define PI5                     131
#define PI6                     132
#define PI7                     133
#define PI8                     134
#define PI9                     135
#define PI10                    136
#define PI11                    137
#define PC2_C                   PIN_A30
#define PC3_C                   PIN_A31

/*----------------------------------------------------------------------------
 * OpenPLC Bridge MPU  -  hardware-signal aliases
 *
 * Sources (Production/ folder):
 *   Bridge/1436_01_SCHAE-BR.pdf          Bridge MPU schematic (STM32H743IIK6)
 *   LowerDeck/Schematics/OpenPLC_LowerDeck_R3.pdf
 *   UpperDeck/Schematics/OpenPLC_UpperDeck_R3.pdf
 *   JunctionLink/Schematics/1434_01_SCHAE-JL.pdf
 *----------------------------------------------------------------------------*/

/* Dual-pad ADC3 pins - direct internal path, better accuracy than normal GPIO */
#ifndef PC2_C
#  define PC2_C   PIN_A30   /* PC2_C - ADC3_INP0 (D138) */
#endif
#ifndef PC3_C
#  define PC3_C   PIN_A31   /* PC3_C - ADC3_INP1 (D139) */
#endif

/*-------- RELAY OUTPUTS - Lower Deck Klemmblock B -------------------------*/
/* Coil drive, active-high (GPIO_PIN_SET = relay closed)                     */
/* Terminals: B01/B02=REL_1  B03/B04=REL_2  ...  B11/B12=REL_6             */
#define REL_1   PI8    /* RELAIS_1                                           */
#define REL_2   PI10   /* RELAIS_2                                           */
#define REL_3   PI11   /* RELAIS_3                                           */
#define REL_4   PG7    /* RELAIS_4                                           */
#define REL_5   PG3    /* RELAIS_5                                           */
#define REL_6   PD3    /* RELAIS_6                                           */

#define REL_OUTA   1   /* coil-A polarity (active-high)                     */
#define REL_OUTB   0   /* coil-B polarity                                   */

/*-------- DIGITAL OUTPUTS - Lower Deck Klemmblock A (high-side FETs) ------*/
/* All PWM-capable via hardware timers (see timer AF column)                 */
/* Active-high: HIGH = FET on = load powered                                 */
/* Terminals: A03=DO1  A04=DO2  ...  A10=DO8                                */
#define DOUT_1  PB13   /* High-side FET 1 - TIM1_CH1N  (PWM)               */
#define DOUT_2  PB0    /* High-side FET 2 - TIM1_CH2N  (PWM)               */
#define DOUT_3  PH15   /* High-side FET 3 - TIM8_CH3N  (PWM)               */
#define DOUT_4  PE4    /* High-side FET 4 - TIM15_CH1N (PWM)               */
#define DOUT_5  PA8    /* High-side FET 5 - TIM1_CH1   (PWM)               */
#define DOUT_6  PA9    /* High-side FET 6 - TIM1_CH2   (PWM)               */
#define DOUT_7  PI7    /* High-side FET 7 - TIM8_CH3   (PWM)               */
#define DOUT_8  PE5    /* High-side FET 8 - TIM15_CH1  (PWM)               */

/*-------- DIGITAL INPUTS - Upper Deck Klemmblock D ------------------------*/
/* All support hardware encoder counter mode (quadrature pairs noted)        */
/* Terminals: D02=DI1  D03=DI2  ...  D09=DI8                                */
#define DIN_1   PC6    /* Digital IN 1 - TIM3_CH1, encoder-1A               */
#define DIN_2   PB5    /* Digital IN 2 - TIM3_CH2, encoder-1B               */
#define DIN_3   PB6    /* Digital IN 3 - TIM4_CH1, encoder-2A               */
#define DIN_4   PB7    /* Digital IN 4 - TIM4_CH2, encoder-2B               */
#define DIN_5   PH10   /* Digital IN 5 - TIM5_CH1, encoder-3A               */
#define DIN_6   PH11   /* Digital IN 6 - TIM5_CH2, encoder-3B               */
#define DIN_7   PI5    /* Digital IN 7 - GPIO input                          */
#define DIN_8   PI6    /* Digital IN 8 - GPIO input                          */

/*-------- ANALOG INPUTS - Upper Deck Klemmblock D -------------------------*/
/* Terminals: D12=AI1  D13=AI2                                               */
#define AIN_1   PC3_C  /* Analog IN 1 - ADC3_INP1 (dual-pad direct path)   */
#define AIN_2   PA6    /* Analog IN 2 - ADC1_INP3                           */

/*-------- ANALOG OUTPUTS - Upper Deck Klemmblock D ------------------------*/
/* Terminals: D14=AO1  D15=AO2                                               */
#define AOUT_1      PA4  /* Analog OUT 1 - DAC1_OUT1                        */
#define AOUT_2      PA5  /* Analog OUT 2 - DAC1_OUT2                        */
#define AOUT_1_EF   PI4  /* Analog OUT 1 fault/enable (active-low)          */
#define AOUT_2_EF   PE3  /* Analog OUT 2 fault/enable (active-low)          */

/*-------- TEMPERATURE SENSORS - Lower Deck --------------------------------*/
#define TEMP_SCPROT  PA0  /* Short-circuit protection NTC - ADC1_INP16      */
#define TEMP_HSSW    PA3  /* High-side FET temperature NTC - ADC1_INP15     */

/*-------- KNX PROGRAMMING INTERFACE - Upper Deck --------------------------*/
/* KNX TP UART (USART1, PB14=TX AF4, PA10=RX AF7) defined in knx_config.h  */
#define KNX_PROG_KEY    PG9    /* Programming button (active-low, EXTI9_5)  */
#define KNX_PROG_LED    PG11   /* Programming LED    (active-high)          */
#define KNX_TP_OK       PD7    /* KNX bus status: HIGH = bus operational    */
#define KNX_TP_VCC_OK   PH12   /* KNX transceiver VCC OK                   */

/*-------- RS232 - Upper Deck Klemmblock C ---------------------------------*/
/* USART3 AF7 - Terminals: C05=TxD, C06=RxD                                */
#define RS232_UART_INSTANCE   3
#define RS232_TX_Pin          PC10  /* USART3_TX → terminal C05             */
#define RS232_RX_Pin          PC11  /* USART3_RX ← terminal C06             */
#define RS232_EN_Pin          PB10  /* RS232 transceiver enable (active-high)*/

/*-------- RS485 - Upper Deck Klemmblock C ---------------------------------*/
/* USART2 AF7 - Terminals: C09=RS485-A/B, C10=Direction                    */
#define RS485_UART_INSTANCE   2
#define RS485_TX_Pin          PD5   /* USART2_TX / RS485 DI                 */
#define RS485_RX_Pin          PD6   /* USART2_RX / RS485 RO                 */
#define RS485_DIR_Pin         PD4   /* RS485 direction (USART2_DE)          */

/*-------- CAN - Upper Deck Klemmblock C -----------------------------------*/
/* FDCAN1 - Terminals: C07=CAN-L, C08=CAN-H                                */
#define CAN_TX_Pin   PB9   /* FDCAN1_TX → terminal C07                     */
#define CAN_RX_Pin   PI9   /* FDCAN1_RX ← terminal C08                     */

/*-------- USB - Bridge MPU ------------------------------------------------*/
/* USB Full-Speed Device (USB-C connector)                                   */
#define USB_DM_Pin   PA11  /* USB_FS2_D_N (D-)                              */
#define USB_DP_Pin   PA12  /* USB_FS2_D_P (D+)                              */

/*-------- SD CARD - Bridge MPU (SDMMC1, 1-bit mode) ----------------------*/
#define SDMMC_CLK_Pin  PC12  /* SDMMC1_CK                                   */
#define SDMMC_CMD_Pin  PD2   /* SDMMC1_CMD                                  */
#define SDMMC_D0_Pin   PC8   /* SDMMC1_D0                                   */
#define SDMMC_CD_Pin   PE6   /* Card detect (active-low)                    */

/*-------- SPI6 - JunctionLink expansion connector -------------------------*/
/* SPI2 is the primary SPI (uses PIN_SPI_* below); SPI6 is secondary        */
#define SPI6_MOSI_Pin  PG14  /* SPI6_MOSI                                   */
#define SPI6_MISO_Pin  PB4   /* SPI6_MISO                                   */
#define SPI6_SCK_Pin   PG13  /* SPI6_SCK                                    */

/*-------- DEBUG -----------------------------------------------------------*/
#define DEBUG_TRIGGER_Pin  PC7  /* Logic-analyser trigger point             */



// Alternate pins number
#define PA0_ALT1                (PA0  | ALT1)
#define PA1_ALT1                (PA1  | ALT1)
#define PA1_ALT2                (PA1  | ALT2)
#define PA2_ALT1                (PA2  | ALT1)
#define PA2_ALT2                (PA2  | ALT2)
#define PA3_ALT1                (PA3  | ALT1)
#define PA3_ALT2                (PA3  | ALT2)
#define PA4_ALT1                (PA4  | ALT1)
#define PA4_ALT2                (PA4  | ALT2)
#define PA5_ALT1                (PA5  | ALT1)
#define PA6_ALT1                (PA6  | ALT1)
#define PA7_ALT1                (PA7  | ALT1)
#define PA7_ALT2                (PA7  | ALT2)
#define PA7_ALT3                (PA7  | ALT3)
#define PA9_ALT1                (PA9  | ALT1)
#define PA10_ALT1               (PA10 | ALT1)
#define PA11_ALT1               (PA11 | ALT1)
#define PA12_ALT1               (PA12 | ALT1)
#define PA15_ALT1               (PA15 | ALT1)
#define PA15_ALT2               (PA15 | ALT2)
#define PB0_ALT1                (PB0  | ALT1)
#define PB0_ALT2                (PB0  | ALT2)
#define PB1_ALT1                (PB1  | ALT1)
#define PB1_ALT2                (PB1  | ALT2)
#define PB3_ALT1                (PB3  | ALT1)
#define PB3_ALT2                (PB3  | ALT2)
#define PB4_ALT1                (PB4  | ALT1)
#define PB4_ALT2                (PB4  | ALT2)
#define PB5_ALT1                (PB5  | ALT1)
#define PB5_ALT2                (PB5  | ALT2)
#define PB6_ALT1                (PB6  | ALT1)
#define PB6_ALT2                (PB6  | ALT2)
#define PB7_ALT1                (PB7  | ALT1)
#define PB8_ALT1                (PB8  | ALT1)
#define PB8_ALT2                (PB8  | ALT2)
#define PB9_ALT1                (PB9  | ALT1)
#define PB9_ALT2                (PB9  | ALT2)
#define PB14_ALT1               (PB14 | ALT1)
#define PB14_ALT2               (PB14 | ALT2)
#define PB15_ALT1               (PB15 | ALT1)
#define PB15_ALT2               (PB15 | ALT2)
#define PC0_ALT1                (PC0  | ALT1)
#define PC0_ALT2                (PC0  | ALT2)
#define PC1_ALT1                (PC1  | ALT1)
#define PC1_ALT2                (PC1  | ALT2)
#define PC4_ALT1                (PC4  | ALT1)
#define PC5_ALT1                (PC5  | ALT1)
#define PC6_ALT1                (PC6  | ALT1)
#define PC6_ALT2                (PC6  | ALT2)
#define PC7_ALT1                (PC7  | ALT1)
#define PC7_ALT2                (PC7  | ALT2)
#define PC8_ALT1                (PC8  | ALT1)
#define PC9_ALT1                (PC9  | ALT1)
#define PC10_ALT1               (PC10 | ALT1)
#define PC11_ALT1               (PC11 | ALT1)
#define PF8_ALT1                (PF8  | ALT1)
#define PF9_ALT1                (PF9  | ALT1)

#define NUM_DIGITAL_PINS        140
#define NUM_DUALPAD_PINS        2
#define NUM_ANALOG_INPUTS       32

// On-board LED pin number
#ifndef LED_BUILTIN
  #define LED_BUILTIN           PNUM_NOT_DEFINED
#endif

// On-board user button
#ifndef USER_BTN
  #define USER_BTN              PNUM_NOT_DEFINED
#endif

// SPI definitions - SPI2 on JunctionLink expansion connector
#ifndef PIN_SPI_SS
  #define PIN_SPI_SS            PI0    /* SPI2_NSS  */
#endif
#ifndef PIN_SPI_SS1
  #define PIN_SPI_SS1           PA15
#endif
#ifndef PIN_SPI_SS2
  #define PIN_SPI_SS2           PG10
#endif
#ifndef PIN_SPI_SS3
  #define PIN_SPI_SS3           PNUM_NOT_DEFINED
#endif
#ifndef PIN_SPI_MOSI
  #define PIN_SPI_MOSI          PI3    /* SPI2_MOSI */
#endif
#ifndef PIN_SPI_MISO
  #define PIN_SPI_MISO          PC2_C  /* SPI2_MISO (dual-pad) */
#endif
#ifndef PIN_SPI_SCK
  #define PIN_SPI_SCK           PI1    /* SPI2_SCK  */
#endif

// I2C definitions - I2C2 on JunctionLink expansion connector (PH4=SCL, PH5=SDA)
#ifndef PIN_WIRE_SDA
  #define PIN_WIRE_SDA          PH5    /* I2C2_SDA (AF4) */
#endif
#ifndef PIN_WIRE_SCL
  #define PIN_WIRE_SCL          PH4    /* I2C2_SCL (AF4) */
#endif

// Timer Definitions
// Use TIM6/TIM7 when possible as servo and tone don't need GPIO output pin
#ifndef TIMER_TONE
  #define TIMER_TONE            TIM6
#endif
#ifndef TIMER_SERVO
  #define TIMER_SERVO           TIM7
#endif

// UART Definitions
// UART4 on JunctionLink expansion connector (PH13=TX, PH14=RX, AF8)
// When USB CDC is selected in Arduino IDE, 'Serial' maps to USB and these
// pins become Serial1 (hardware UART4).
#ifndef SERIAL_UART_INSTANCE
  #define SERIAL_UART_INSTANCE  4
#endif
#ifndef PIN_SERIAL_RX
  #define PIN_SERIAL_RX         PH14   /* UART4_RX on JunctionLink connector */
#endif
#ifndef PIN_SERIAL_TX
  #define PIN_SERIAL_TX         PH13   /* UART4_TX on JunctionLink connector */
#endif

// Extra HAL modules
#if !defined(HAL_DAC_MODULE_DISABLED)
  #define HAL_DAC_MODULE_ENABLED
#endif

#if !defined(HAL_ADC_MODULE_DISABLED)
  #define HAL_ADC_MODULE_ENABLED
#endif

#if !defined(HAL_ETH_MODULE_DISABLED)
  #define HAL_ETH_MODULE_ENABLED
#endif
#if !defined(HAL_QSPI_MODULE_DISABLED)
  #define HAL_QSPI_MODULE_ENABLED
#endif
#if !defined(HAL_SD_MODULE_DISABLED)
  #define HAL_SD_MODULE_ENABLED
#endif

/*----------------------------------------------------------------------------
 *        Arduino objects - C++ only
 *----------------------------------------------------------------------------*/

#ifdef __cplusplus
  // These serial port names are intended to allow libraries and architecture-neutral
  // sketches to automatically default to the correct port name for a particular type
  // of use.  For example, a GPS module would normally connect to SERIAL_PORT_HARDWARE_OPEN,
  // the first hardware serial port whose RX/TX pins are not dedicated to another use.
  //
  // SERIAL_PORT_MONITOR        Port which normally prints to the Arduino Serial Monitor
  //
  // SERIAL_PORT_USBVIRTUAL     Port which is USB virtual serial
  //
  // SERIAL_PORT_LINUXBRIDGE    Port which connects to a Linux system via Bridge library
  //
  // SERIAL_PORT_HARDWARE       Hardware serial port, physical RX & TX pins.
  //
  // SERIAL_PORT_HARDWARE_OPEN  Hardware serial ports which are open for use.  Their RX & TX
  //                            pins are NOT connected to anything by default.
  #ifndef SERIAL_PORT_MONITOR
  #define SERIAL_PORT_MONITOR   Serial
  #endif

  #ifndef SERIAL_PORT_HARDWARE
    #define SERIAL_PORT_HARDWARE  Serial
  #endif
#endif
