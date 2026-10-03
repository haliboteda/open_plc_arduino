/*
 * T3-08: the reset cause the bootloader publishes reaches the sketch.
 * Runs the core's copy of IAP_boot_handoff.c (mirror of the bootloader's, P2)
 * for the bootloader half, then openplc_reset_cause() for the sketch half.
 * One scenario per process: boot_handoff_take() runs once per boot.
 * Decision 80.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "stm32_def.h"
#include "IAP_boot_handoff.h"
#include "openplc_reset.h"

RCC_TypeDef fake_rcc;
uint32_t fake_sram4[8];
void HAL_NVIC_SystemReset(void) { abort(); }

static int failures;

static void check(int ok, const char *what)
{
	printf("[%s] %s\n", ok ? "PASS" : "FAIL", what);
	if (!ok) {
		failures++;
	}
}

static void boot(uint32_t rsr)
{
	fake_rcc.RSR = rsr;
	(void)boot_handoff_take();   /* the bootloader's every-boot step */
}

int main(int argc, char **argv)
{
	const char *s = (argc > 1) ? argv[1] : "";

	if (strcmp(s, "watchdog") == 0) {
		/* A watchdog reset also pulses NRST. */
		boot(RCC_RSR_IWDG1RSTF | RCC_RSR_PINRSTF);
		check(fake_rcc.RSR == 0U, "the bootloader cleared RCC->RSR (why IWatchdog::isReset() cannot work)");
		check(openplc_reset_cause() == OPENPLC_RESET_WATCHDOG, "sketch sees a watchdog reset");
	} else if (strcmp(s, "poweron") == 0) {
		memset(fake_sram4, 0xA5, sizeof(fake_sram4));   /* power-on leaves SRAM4 random */
		boot(RCC_RSR_PORRSTF | RCC_RSR_PINRSTF | RCC_RSR_BORRSTF);
		check(openplc_reset_cause() == OPENPLC_RESET_POWER_ON, "sketch sees a power-on reset");
	} else if (strcmp(s, "software") == 0) {
		boot(RCC_RSR_SFTRSTF | RCC_RSR_PINRSTF);
		check(openplc_reset_cause() == OPENPLC_RESET_SOFTWARE, "sketch sees a software reset");
	} else if (strcmp(s, "pin") == 0) {
		boot(RCC_RSR_PINRSTF);
		check(openplc_reset_cause() == OPENPLC_RESET_PIN, "sketch sees the reset button");
	} else if (strcmp(s, "corrupt") == 0) {
		boot(RCC_RSR_IWDG1RSTF | RCC_RSR_PINRSTF);
		fake_sram4[5] ^= 1U;   /* the check word */
		check(openplc_reset_cause() == OPENPLC_RESET_UNKNOWN, "a damaged record reads as unknown");
	} else if (strcmp(s, "blank") == 0) {
		check(openplc_reset_cause() == OPENPLC_RESET_UNKNOWN, "nothing published reads as unknown");
	} else {
		printf("usage: reset_cause_test watchdog|poweron|software|pin|corrupt|blank\n");
		return 2;
	}
	return failures ? 1 : 0;
}
