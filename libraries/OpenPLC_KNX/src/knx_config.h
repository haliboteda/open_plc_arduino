#pragma once

/*
 * knx_config.h - Compile-time hardware configuration for OpenPLC_KNX.
 *
 * All pin references use STM32 HAL GPIO port/pin constants.
 * No Arduino.h dependency - safe to include from pure-HAL translation units.
 *
 * Override any macro via compiler flag (e.g. -DKNX_USART_BAUD=19200)
 * before this header is processed.
 */

#include <stm32h7xx_hal.h>

/* -----------------------------------------------------------------------
 * KNX TP - STKNX transceiver via USART1
 *   TX = PB14  (AF4)   → STKNX via TLP2362 optocoupler
 *   RX = PA10  (AF7)   ← STKNX via TLP2362 optocoupler
 * --------------------------------------------------------------------- */
#ifndef KNX_USART_INSTANCE
#  define KNX_USART_INSTANCE    USART1
#endif
#ifndef KNX_USART_IRQn
#  define KNX_USART_IRQn        USART1_IRQn
#endif
#ifndef KNX_USART_BAUD
#  define KNX_USART_BAUD        19200u
#endif
#ifndef KNX_USART_TX_PORT
#  define KNX_USART_TX_PORT     GPIOB
#  define KNX_USART_TX_PIN      GPIO_PIN_14
#  define KNX_USART_TX_AF       GPIO_AF4_USART1
#endif
#ifndef KNX_USART_RX_PORT
#  define KNX_USART_RX_PORT     GPIOA
#  define KNX_USART_RX_PIN      GPIO_PIN_10
#  define KNX_USART_RX_AF       GPIO_AF7_USART1
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

/* Programming mode button (active-low) and LED (active-high) */
#ifndef KNX_PROG_KEY_PORT
#  define KNX_PROG_KEY_PORT     GPIOG
#  define KNX_PROG_KEY_PIN      GPIO_PIN_9
#  define KNX_PROG_KEY_IRQn     EXTI9_5_IRQn
#endif
#ifndef KNX_PROG_LED_PORT
#  define KNX_PROG_LED_PORT     GPIOG
#  define KNX_PROG_LED_PIN      GPIO_PIN_11
#endif

/* -----------------------------------------------------------------------
 * Relay outputs - OpenPLC Bridge MPU schematic
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
 * Flash NVM layout (STM32H743, dual-bank, 128 KB / sector)
 *
 *   Bank 2, Sector 6  0x081C0000  KNX stack NVM (reference library ETS data)
 *   Bank 2, Sector 7  0x081E0000  Application NVM (KnxNvmConfig)
 * --------------------------------------------------------------------- */
#ifndef KNX_STACK_NVM_FLASH_ADDR
#  define KNX_STACK_NVM_FLASH_ADDR    0x081C0000U
#  define KNX_STACK_NVM_FLASH_BANK    FLASH_BANK_2
#  define KNX_STACK_NVM_FLASH_SECTOR  FLASH_SECTOR_6
#endif
#ifndef KNX_APP_NVM_FLASH_ADDR
#  define KNX_APP_NVM_FLASH_ADDR      0x081E0000U
#  define KNX_APP_NVM_FLASH_BANK      FLASH_BANK_2
#  define KNX_APP_NVM_FLASH_SECTOR    FLASH_SECTOR_7
#endif
/* Reference library NVM size in bytes (must be multiple of 32 for H7 programming) */
#ifndef KNX_FLASH_SIZE
#  define KNX_FLASH_SIZE              4096u
#endif

/* -----------------------------------------------------------------------
 * UART receive ring buffer
 * --------------------------------------------------------------------- */
#ifndef KNX_UART_RXBUF_SIZE
#  define KNX_UART_RXBUF_SIZE         256u
#endif

/* -----------------------------------------------------------------------
 * IP receive staging buffer (one UDP datagram at a time)
 * --------------------------------------------------------------------- */
#ifndef KNX_IP_RXBUF_SIZE
#  define KNX_IP_RXBUF_SIZE           512u
#endif

/* -----------------------------------------------------------------------
 * NVIC interrupt priority for USART1 (lower number = higher priority).
 * Must be >= configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY if FreeRTOS is used.
 * --------------------------------------------------------------------- */
#ifndef KNX_UART_IRQ_PRIORITY
#  define KNX_UART_IRQ_PRIORITY       5u
#endif

/* -----------------------------------------------------------------------
 * Default KNX individual address used when no ETS programming has been
 * performed yet (1.1.1 = area 1, line 1, device 1).
 * Used by both setup() NVM fallback and selfProgram2CH() default argument.
 * --------------------------------------------------------------------- */
#ifndef KNX_DEFAULT_INDIVIDUAL_ADDR
#  define KNX_DEFAULT_INDIVIDUAL_ADDR  0x1101u
#endif
