#pragma once

/*
 * knx_config.h - Compile-time hardware configuration for OpenPLC_KNX.
 *
 * All pin references use STM32 HAL GPIO port/pin constants.
 * No Arduino.h dependency - safe to include from pure-HAL translation units.
 *
 * Override any macro via compiler flag (e.g. -DKNX_TP_IRQ_PRIORITY=3)
 * before this header is processed.
 */

#include <stm32h7xx_hal.h>

/* -----------------------------------------------------------------------
 * KNX TP - STKNX bare TP1 transceiver; the bits are made by two timers
 * (stknx_phy.cpp). Pins and polarity: $PROD/docs/hardware/HARDWARE-FACTS.md
 * "KNX 接口"; design: $PROD/docs/modules/M3/KNX-TP-DATA-LINK.md
 * --------------------------------------------------------------------- */
#ifndef KNX_TP_TX_TIM
#  define KNX_TP_TX_TIM         TIM12     /* CH1 drives KNX_TX */
#  define KNX_TP_TX_PORT        GPIOB
#  define KNX_TP_TX_PIN         GPIO_PIN_14
#  define KNX_TP_TX_AF          GPIO_AF2_TIM12
#endif
#ifndef KNX_TP_RX_TIM
#  define KNX_TP_RX_TIM         TIM1      /* CH3 captures KNX_RX */
#  define KNX_TP_RX_PORT        GPIOA
#  define KNX_TP_RX_PIN         GPIO_PIN_10
#  define KNX_TP_RX_AF          GPIO_AF1_TIM1
#endif
/* Both timer interrupts share one priority so they never preempt each other. */
#ifndef KNX_TP_IRQ_PRIORITY
#  define KNX_TP_IRQ_PRIORITY   2u
#endif

/* STKNX status GPIO */
#ifndef KNX_TP_OK_PORT
#  define KNX_TP_OK_PORT        GPIOD     /* STKNX pin 21: high = bus operational */
#  define KNX_TP_OK_PIN         GPIO_PIN_7
#endif
#ifndef KNX_TP_VCC_OK_PORT
#  define KNX_TP_VCC_OK_PORT    GPIOH     /* STKNX pin 19: high = chip powered */
#  define KNX_TP_VCC_OK_PIN     GPIO_PIN_12
#endif

/* Programming button: pressed = high, external 10k pull-down; the net is
 * also BOOT0. Programming-mode indicator: the system LED on PE2 (high = on),
 * since the KNX_Prog_LED line (PG11) has no LED fitted.
 * $PROD/docs/hardware/HARDWARE-FACTS.md "PG9 就是 BOOT0 网", "KNX 接口" */
#ifndef KNX_PROG_KEY_PORT
#  define KNX_PROG_KEY_PORT     GPIOG
#  define KNX_PROG_KEY_PIN      GPIO_PIN_9
#  define KNX_PROG_KEY_IRQn     EXTI9_5_IRQn
#endif
#ifndef KNX_PROG_LED_PORT
#  define KNX_PROG_LED_PORT     GPIOE
#  define KNX_PROG_LED_PIN      GPIO_PIN_2
#endif

/* -----------------------------------------------------------------------
 * Relay outputs - Lower Deck relays 1 and 2 (terminals B01-B04)
 * --------------------------------------------------------------------- */
#ifndef KNX_RELAY1_PORT
#  define KNX_RELAY1_PORT       GPIOI
#  define KNX_RELAY1_PIN        GPIO_PIN_8    /* REL_1 = PI8  */
#  define KNX_RELAY2_PORT       GPIOI
#  define KNX_RELAY2_PIN        GPIO_PIN_10   /* REL_2 = PI10 */
#endif
/* Relay active state: GPIO_PIN_SET = coil energised (relay closed) */
#ifndef KNX_RELAY_ACTIVE
#  define KNX_RELAY_ACTIVE      GPIO_PIN_SET
#  define KNX_RELAY_INACTIVE    GPIO_PIN_RESET
#endif

/* -----------------------------------------------------------------------
 * KNXnet/IP - UDP multicast
 * --------------------------------------------------------------------- */
#ifndef KNX_IP_PORT
#  define KNX_IP_PORT           3671u
#endif

/* -----------------------------------------------------------------------
 * Flash NVM layout: both blocks share Bank 2 Sector 6 (0x081C0000), the last
 * sector of the application area. Sector 7 belongs to the bootloader and must
 * never be written from an application.
 *
 *   +0                  KNX stack NVM (reference library ETS data), KNX_FLASH_SIZE
 *   +KNX_FLASH_SIZE     Application NVM (KnxNvmConfig)
 *
 * One erase covers both, so every save rewrites both (knx_nvm_sector_write).
 * A sketch linking this library must therefore end below 0x081C0000; the
 * board package's postbuild step enforces it.
 * --------------------------------------------------------------------- */
/* Reference library NVM size in bytes (must be multiple of 32 for H7 programming) */
#ifndef KNX_FLASH_SIZE
#  define KNX_FLASH_SIZE              4096u
#endif
#define KNX_NVM_FLASH_ADDR            0x081C0000U
#define KNX_NVM_FLASH_BANK            FLASH_BANK_2
#define KNX_NVM_FLASH_SECTOR          FLASH_SECTOR_6
#define KNX_STACK_NVM_FLASH_ADDR      KNX_NVM_FLASH_ADDR
#define KNX_APP_NVM_FLASH_ADDR        (KNX_NVM_FLASH_ADDR + KNX_FLASH_SIZE)

/* -----------------------------------------------------------------------
 * IP receive staging buffer (one UDP datagram at a time)
 * --------------------------------------------------------------------- */
#ifndef KNX_IP_RXBUF_SIZE
#  define KNX_IP_RXBUF_SIZE           512u
#endif

/* -----------------------------------------------------------------------
 * Default KNX individual address used when no ETS programming has been
 * performed yet (1.1.1 = area 1, line 1, device 1).
 * Used by both setup() NVM fallback and selfProgram2CH() default argument.
 * --------------------------------------------------------------------- */
#ifndef KNX_DEFAULT_INDIVIDUAL_ADDR
#  define KNX_DEFAULT_INDIVIDUAL_ADDR  0x1101u
#endif
