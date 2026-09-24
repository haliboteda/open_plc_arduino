/*
 * openplc_rng.c
 *
 * See openplc_rng.h.
 */

#include "openplc_rng.h"
#include "stm32_def.h"
#include <stdlib.h>

/* The Arduino core brings up no RNG of its own, so this file owns the handle and
 * the clock. The kernel clock is HSI48, which SystemClock_Config() already turns
 * on for USB (variants/STM32H7xx/H743/generic_clock.c) -- a sketch that replaces
 * that WEAK function has to keep it. */
static RNG_HandleTypeDef s_hrng;
static bool s_rng_ready;

bool openplc_rng_words(uint32_t *words, uint32_t n)
{
	uint32_t i;

	if (!s_rng_ready) {
		RCC_PeriphCLKInitTypeDef clk = {0};

		clk.PeriphClockSelection = RCC_PERIPHCLK_RNG;
		clk.RngClockSelection = RCC_RNGCLKSOURCE_HSI48;
		if (HAL_RCCEx_PeriphCLKConfig(&clk) != HAL_OK) {
			return false;
		}
		__HAL_RCC_RNG_CLK_ENABLE();

		s_hrng.Instance = RNG;
		s_hrng.Init.ClockErrorDetection = RNG_CED_ENABLE;
		if (HAL_RNG_Init(&s_hrng) != HAL_OK) {
			return false;
		}
		s_rng_ready = true;
	}

	for (i = 0; i < n; i++) {
		if (HAL_RNG_GenerateRandomNumber(&s_hrng, &words[i]) != HAL_OK) {
			return false;
		}
	}
	return true;
}

void openplc_rng_seed_rand(void)
{
	uint32_t seed;

	if (openplc_rng_words(&seed, 1U)) {
		srand(seed);
	}
}

uint32_t openplc_rng_tcp_isn(void)
{
	uint32_t isn;

	if (!openplc_rng_words(&isn, 1U)) {
		isn = (uint32_t)rand();
	}
	return isn;
}
