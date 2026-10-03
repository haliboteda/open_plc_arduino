/*
 * stknx_phy.cpp - see stknx_phy.h.
 *
 * Goes through the core's HardwareTimer because the core already owns
 * TIM1_CC_IRQHandler and the TIM12 handler.
 */

#include <Arduino.h>
#include "stknx_phy.h"
#include "knx_config.h"

static HardwareTimer *s_tx;
static HardwareTimer *s_rx;
static stknx_link_t  *s_link;
static uint32_t       s_ticks_per_us;

/* Start of a bit period. CCR1 and ARR are preloaded, so what is written here
 * shapes the period after this one and never glitches the one under way. */
static void on_bit(void)
{
    uint16_t len  = STKNX_BIT_US;
    uint16_t now  = (uint16_t)KNX_TP_RX_TIM->CNT;
    uint8_t  emit = stknx_link_tick(s_link, now, &len);

    KNX_TP_TX_TIM->CCR1 = emit ? (STKNX_PULSE_US * s_ticks_per_us) : 0u;
    KNX_TP_TX_TIM->ARR  = ((uint32_t)len * s_ticks_per_us) - 1u;
}

static void on_pulse(void)
{
    uint16_t t = (uint16_t)KNX_TP_RX_TIM->CCR3;

    if (stknx_link_pulse(s_link, t)) {
        KNX_TP_TX_TIM->CCR1 = 0u;   /* arbitration lost: next period silent */
    }
}

void stknx_phy_park(void)
{
    GPIO_InitTypeDef g = {};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    HAL_GPIO_WritePin(KNX_TP_TX_PORT, KNX_TP_TX_PIN, GPIO_PIN_RESET);
    g.Pin   = KNX_TP_TX_PIN;
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(KNX_TP_TX_PORT, &g);
}

bool stknx_phy_start(stknx_link_t *l)
{
    GPIO_InitTypeDef g = {};

    if (s_link != nullptr) {
        return false;
    }
    stknx_phy_park();
    s_link = l;

    if (s_tx == nullptr) {
        s_tx = new HardwareTimer(KNX_TP_TX_TIM);
        s_rx = new HardwareTimer(KNX_TP_RX_TIM);
    }

    /* Receive: 1 us per count, falling edge = start of an active pulse. */
    s_rx->pause();
    s_rx->setPrescaleFactor(s_rx->getTimerClkFreq() / 1000000u);
    s_rx->setOverflow(0x10000u, TICK_FORMAT);
    s_rx->setMode(3, TIMER_INPUT_CAPTURE_FALLING, NC);
    s_rx->attachInterrupt(3, on_pulse);
    s_rx->setInterruptPriority(KNX_TP_IRQ_PRIORITY, 0);

    /* Transmit: PWM mode 1, so CCR1 is the pulse width and 0 holds the line low. */
    s_ticks_per_us = s_tx->getTimerClkFreq() / 1000000u;
    s_tx->pause();
    s_tx->setPrescaleFactor(1);
    s_tx->setOverflow(STKNX_BIT_US * s_ticks_per_us, TICK_FORMAT);
    s_tx->setMode(1, TIMER_OUTPUT_COMPARE_PWM1, NC);
    s_tx->setCaptureCompare(1, 0, TICK_COMPARE_FORMAT);
    s_tx->attachInterrupt(on_bit);
    s_tx->setInterruptPriority(KNX_TP_IRQ_PRIORITY, 0);

    s_rx->resume();
    s_tx->resume();

    /* Pins last: the channel is already driving low when PB14 is handed over. */
    g.Pin       = KNX_TP_RX_PIN;
    g.Mode      = GPIO_MODE_AF_PP;
    g.Pull      = GPIO_PULLUP;   /* no pull-up on the board for this line */
    g.Speed     = GPIO_SPEED_FREQ_HIGH;
    g.Alternate = KNX_TP_RX_AF;
    __HAL_RCC_GPIOA_CLK_ENABLE();
    HAL_GPIO_Init(KNX_TP_RX_PORT, &g);

    g.Pin       = KNX_TP_TX_PIN;
    g.Pull      = GPIO_NOPULL;
    g.Alternate = KNX_TP_TX_AF;
    HAL_GPIO_Init(KNX_TP_TX_PORT, &g);
    return true;
}

void stknx_phy_stop(void)
{
    if (s_link == nullptr) {
        return;
    }
    stknx_phy_park();
    s_tx->pause();
    s_rx->pause();
    s_tx->detachInterrupt();
    s_rx->detachInterrupt(3);
    s_link = nullptr;
}
