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
 * Flash NVM layout: Bank 2 Sector 6 (0x081C0000), the last sector of the
 * application area, holds only the KNX stack NVM (the ETS data: individual
 * address and tables), KNX_FLASH_SIZE bytes at its start. Sector 7 belongs to
 * the bootloader and must never be written from an application.
 *
 * KNX_FLASH_SIZE is 16 KiB for every role, set with MASK_VERSION by the header
 * prebuild.sh generates from OPENPLC_KNX_ROLE() (force-included into every
 * unit, so the stack's own translation units see it too): the 0x091A filter table
 * alone needs 512 + 8192 B, the rest holds 0x07B0 links.
 * $PROD/docs/build/BOOTLOADER-PROJECT-LAYOUT.md
 *
 * Every save erases the sector and rewrites the block (knx_nvm_sector_write).
 * A sketch linking this library must therefore end below 0x081C0000; the
 * board package's postbuild step enforces it.
 * --------------------------------------------------------------------- */
#ifndef KNX_FLASH_SIZE
#  error "OpenPLC_KNX: no KNX role (KNX_FLASH_SIZE undefined). Add OPENPLC_KNX_ROLE(TP_DEVICE); or OPENPLC_KNX_ROLE(IP_ROUTER); on its own line in the .ino file"
#endif
#define KNX_NVM_FLASH_ADDR            0x081C0000U
#define KNX_NVM_FLASH_BANK            FLASH_BANK_2
#define KNX_NVM_FLASH_SECTOR          FLASH_SECTOR_6
#define KNX_STACK_NVM_FLASH_ADDR      KNX_NVM_FLASH_ADDR

/* -----------------------------------------------------------------------
 * IP receive staging buffer (one UDP datagram at a time)
 * --------------------------------------------------------------------- */
#ifndef KNX_IP_RXBUF_SIZE
#  define KNX_IP_RXBUF_SIZE           512u
#endif

/* -----------------------------------------------------------------------
 * Default KNX individual address used when no ETS programming has been
 * performed yet (1.1.1 = area 1, line 1, device 1). KNX.setup() sets it before
 * the stack reads its NVM, which replaces it with the ETS-programmed one if any.
 * --------------------------------------------------------------------- */
#ifndef KNX_DEFAULT_INDIVIDUAL_ADDR
#  define KNX_DEFAULT_INDIVIDUAL_ADDR  0x1101u
#endif
