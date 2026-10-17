/*
 * The SRAM4 boot-log ring (cores/arduino/stm32/boot_log.c, mirror of the
 * bootloader's, P2) on a RAM buffer. One scenario per process.
 * Format: $PROD/docs/modules/M1/BOOT-LOG.md.
 */
#include <stdio.h>
#include <string.h>
#include "stm32_def.h"
#include "boot_log.h"

uint32_t fake_sram4_log[1024];
uint32_t fake_primask;

#define MAX_LINES 1000

static struct {
	unsigned n;
	char     text[MAX_LINES][BOOT_LOG_LINE_MAX + 1];
	uint32_t seq[MAX_LINES];
} got;

static int failures;

static void check(int ok, const char *what)
{
	printf("[%s] %s\n", ok ? "PASS" : "FAIL", what);
	if (!ok) {
		failures++;
	}
}

static void collect(const char *line, size_t len, uint32_t seq, void *ctx)
{
	(void)ctx;
	if ((got.n < MAX_LINES) && (strlen(line) == len)) {
		memcpy(got.text[got.n], line, len + 1U);
		got.seq[got.n] = seq;
	}
	got.n++;
}

static void read_all(void)
{
	memset(&got, 0, sizeof(got));
	boot_log_read_all(collect, NULL);
}

static uint32_t read_unsent(void)
{
	memset(&got, 0, sizeof(got));
	return boot_log_read_unsent(collect, NULL);
}

static void put(const char *s)
{
	(void)boot_log_write(s, strlen(s));
}

static boot_log_hdr_t *hdr(void)
{
	return (boot_log_hdr_t *)fake_sram4_log;
}

static uint8_t *ring(void)
{
	return (uint8_t *)fake_sram4_log + BOOT_LOG_HDR_SIZE;
}

static int only_corrupt_line(void)
{
	read_all();
	return (got.n == 1U) && (strcmp(got.text[0], "[sys][LOG] buffer reset: corrupt") == 0);
}

int main(int argc, char **argv)
{
	const char *s = (argc > 1) ? argv[1] : "";

	if (strcmp(s, "garbage") == 0) {
		memset(fake_sram4_log, 0x5A, sizeof(fake_sram4_log));
		boot_log_init(false);
		read_all();
		check(got.n == 0U, "wrong magic: initialised silently, empty");
		check(hdr()->magic == BOOT_LOG_MAGIC, "header written");
		put("[sys][BL] x");
		boot_log_init(true);
		read_all();
		check(got.n == 0U, "cold init empties a valid buffer without reading it");
	} else if (strcmp(s, "write_read") == 0) {
		boot_log_init(true);
		put("[sys][BL] Reset cause: POR");
		put("[sys][BOOT] millis=12\r\n");
		check(fake_primask == 0U, "interrupt mask restored after a write");
		boot_log_init(false);   /* warm reset: bootloader, then app */
		read_all();
		check(got.n == 2U, "both lines survive a warm init");
		check(strcmp(got.text[0], "[sys][BL] Reset cause: POR") == 0 && got.seq[0] == 0U, "first line, seq 0");
		check(strcmp(got.text[1], "[sys][BOOT] millis=12") == 0 && got.seq[1] == 1U, "trailing CR/LF dropped, seq 1");
	} else if (strcmp(s, "wrap") == 0) {
		char line[101];
		unsigned i;
		int ok = 1;

		boot_log_init(true);
		for (i = 0U; i < 300U; i++) {
			snprintf(line, sizeof(line), "%03u%097u", i, 0U);
			put(line);
		}
		boot_log_init(false);
		read_all();
		check(got.n > 30U && got.n < 300U, "oldest records dropped");
		check(got.seq[got.n - 1U] == 299U && strncmp(got.text[got.n - 1U], "299", 3) == 0, "newest record kept");
		for (i = 1U; i < got.n; i++) {
			ok = ok && (got.seq[i] == got.seq[i - 1U] + 1U);
		}
		check(ok && strlen(got.text[0]) == 100U, "whole records only, consecutive, valid after wraparound");
	} else if (strcmp(s, "split") == 0) {
		char line[451];
		char joined[451] = "";
		unsigned i;

		for (i = 0U; i < 450U; i++) {
			line[i] = (char)('a' + (i % 26U));
		}
		line[450] = '\0';
		boot_log_init(true);
		put(line);
		read_all();
		check(got.n == 3U, "450 bytes become three records");
		check(strlen(got.text[0]) == 200U && strlen(got.text[1]) == 200U && strlen(got.text[2]) == 50U, "200 + 200 + 50");
		for (i = 0U; i < got.n && i < 3U; i++) {
			strcat(joined, got.text[i]);
		}
		check(strcmp(joined, line) == 0, "pieces join back to the line");
	} else if (strcmp(s, "corrupt") == 0) {
		boot_log_init(true);
		put("hello");
		ring()[4] ^= 0x01U;   /* first text byte: CRC no longer matches */
		boot_log_init(false);
		check(only_corrupt_line(), "bad record: reset, one LOG line");

		put("hello");
		hdr()->tail ^= 1U;     /* check word no longer matches */
		boot_log_init(false);
		check(only_corrupt_line(), "bad header check: reset, one LOG line");

		hdr()->version = (uint16_t)(BOOT_LOG_VERSION + 1U);
		hdr()->check = ~(hdr()->magic ^ hdr()->version ^ hdr()->size ^ hdr()->head ^
		                 hdr()->tail ^ hdr()->seq ^ hdr()->sent);
		boot_log_init(false);
		check(only_corrupt_line(), "newer version: reset, one LOG line");
	} else if (strcmp(s, "sent") == 0) {
		uint32_t upto;

		boot_log_init(true);
		put("a");
		put("b");
		put("c");
		upto = read_unsent();
		check(got.n == 3U && upto == 3U, "three unsent lines");
		boot_log_mark_sent(upto);
		put("d");
		boot_log_init(false);
		upto = read_unsent();
		check(got.n == 1U && strcmp(got.text[0], "d") == 0 && upto == 4U, "only the new line is unsent, across a warm init");
		read_all();
		check(got.n == 4U, "read_all ignores sent");
		boot_log_mark_sent(2U);
		check(hdr()->sent == 3U, "sent never moves backwards");
	} else if (strcmp(s, "unsent_dropped") == 0) {
		char line[101];
		unsigned i;

		boot_log_init(true);
		for (i = 0U; i < 100U; i++) {
			snprintf(line, sizeof(line), "%0100u", i);
			put(line);
		}
		(void)read_unsent();
		check(got.n > 0U && got.seq[0] > 0U && got.seq[got.n - 1U] == 99U, "unsent lines already dropped are skipped");
	} else {
		printf("usage: boot_log_test garbage|write_read|wrap|split|corrupt|sent|unsent_dropped\n");
		return 2;
	}
	return failures ? 1 : 0;
}
