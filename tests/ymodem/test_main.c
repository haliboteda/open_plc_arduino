/*
 * T3-10: the YMODEM receiver (real openplc_ymodem.c) against scripted senders.
 *
 *   ymodem_test <scenario>
 *
 * The sender is a fixed byte script; -1 in it is a read timeout. Every byte the
 * receiver writes and every file callback is recorded and checked.
 */
#include "openplc_ymodem.h"

#include <stdio.h>
#include <string.h>

#define SOH 0x01
#define STX 0x02
#define EOT 0x04
#define ACK 0x06
#define NAK 0x15
#define CAN 0x18

static int failures;

static void check(int ok, const char *what)
{
	printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
	if (!ok) {
		failures++;
	}
}

/* --- the scripted line ----------------------------------------------------- */

static int script[20000];
static int nscript, pos;
static uint8_t sent[256];
static int nsent;

static uint8_t file[8192];
static uint32_t file_len;
static char file_name[80];
static uint32_t file_size;
static int opened, closed, closed_ok, refuse_open;

static int rd(void *ctx, uint32_t t)
{
	(void)ctx;
	(void)t;
	return (pos < nscript) ? script[pos++] : -1;
}

static void wr(void *ctx, uint8_t b)
{
	(void)ctx;
	if (nsent < (int)sizeof(sent)) {
		sent[nsent++] = b;
	}
}

static bool op(void *ctx, const char *name, uint32_t size)
{
	(void)ctx;
	snprintf(file_name, sizeof(file_name), "%s", name);
	file_size = size;
	opened++;
	return !refuse_open;
}

static bool da(void *ctx, const uint8_t *buf, uint32_t len)
{
	(void)ctx;
	memcpy(&file[file_len], buf, len);
	file_len += len;
	return true;
}

static void cl(void *ctx, bool ok)
{
	(void)ctx;
	closed++;
	closed_ok = ok;
}

static const openplc_ymodem_io_t IO = { 0, rd, wr, op, da, cl };

/* --- building the sender's bytes ------------------------------------------- */

static uint16_t crc16(const uint8_t *p, int n)
{
	uint16_t c = 0;
	while (n-- > 0) {
		c ^= (uint16_t)(*p++ << 8);
		for (int i = 0; i < 8; i++) {
			c = (c & 0x8000) ? (uint16_t)((c << 1) ^ 0x1021) : (uint16_t)(c << 1);
		}
	}
	return c;
}

static void put(int b) { script[nscript++] = b; }

static void packet(int seq, const uint8_t *data, int size, int corrupt)
{
	uint8_t d[1024];
	memset(d, 0x1A, sizeof(d));
	memcpy(d, data, (size_t)size);
	int len = (size > 128) ? 1024 : 128;
	if (seq == 0) {
		memset(d + size, 0, (size_t)(len - size));
	}
	uint16_t c = crc16(d, len);
	put(len == 1024 ? STX : SOH);
	put(seq & 0xFF);
	put(~seq & 0xFF);
	for (int i = 0; i < len; i++) {
		put((i == 5 && corrupt) ? (d[i] ^ 0xFF) : d[i]);
	}
	put(c >> 8);
	put(c & 0xFF);
}

static void header(const char *name, uint32_t size)
{
	uint8_t d[128] = {0};
	int n = (int)strlen(name);
	memcpy(d, name, (size_t)n);
	n += 1 + snprintf((char *)d + n + 1, 40, "%lu 0", (unsigned long)size);
	packet(0, d, n, 0);
}

static void end_of_file(void) { put(EOT); put(EOT); }
static void end_of_batch(void) { packet(0, (const uint8_t *)"", 0, 0); }

static uint8_t pattern[4096];

static void reset(void)
{
	nscript = pos = nsent = 0;
	file_len = file_size = 0;
	opened = closed = closed_ok = refuse_open = 0;
	file_name[0] = '\0';
	for (int i = 0; i < (int)sizeof(pattern); i++) {
		pattern[i] = (uint8_t)(i * 7 + 3);
	}
}

static int sent_is(const uint8_t *want, int n)
{
	return nsent == n && memcmp(sent, want, (size_t)n) == 0;
}

/* --- scenarios ------------------------------------------------------------- */

static void t_one_file(void)
{
	header("data.bin", 1500);
	packet(1, pattern, 1024, 0);
	packet(2, pattern + 1024, 476, 0);
	end_of_file();
	end_of_batch();
	openplc_ymodem_result_t r = openplcYmodemReceive(&IO, 3000);
	static const uint8_t want[] = { 'C', ACK, 'C', ACK, ACK, NAK, ACK, 'C', ACK };
	check(r == OPENPLC_YMODEM_DONE, "a one-file batch ends DONE");
	check(strcmp(file_name, "data.bin") == 0 && file_size == 1500, "name and length come from the first packet");
	check(file_len == 1500 && memcmp(file, pattern, 1500) == 0, "the file is the bytes sent, cut to its length");
	check(closed == 1 && closed_ok, "the file is closed once, complete");
	check(sent_is(want, sizeof(want)), "answers: C, ACK, C, ACK per packet, NAK then ACK on EOT, C, ACK");
}

static void t_short_packets(void)
{
	header("s.txt", 200);
	packet(1, pattern, 128, 0);
	packet(2, pattern + 128, 72, 0);
	end_of_file();
	end_of_batch();
	openplc_ymodem_result_t r = openplcYmodemReceive(&IO, 3000);
	check(r == OPENPLC_YMODEM_DONE && file_len == 200 && memcmp(file, pattern, 200) == 0,
	      "128-byte packets are taken too");
}

static void t_bad_crc(void)
{
	header("c.bin", 100);
	packet(1, pattern, 100, 1);
	put(-1);   /* the sender waits for our answer */
	packet(1, pattern, 100, 0);
	end_of_file();
	end_of_batch();
	openplc_ymodem_result_t r = openplcYmodemReceive(&IO, 3000);
	static const uint8_t want[] = { 'C', ACK, 'C', NAK, ACK, NAK, ACK, 'C', ACK };
	check(r == OPENPLC_YMODEM_DONE && file_len == 100 && memcmp(file, pattern, 100) == 0,
	      "a packet with a bad CRC is asked for again and written once");
	check(sent_is(want, sizeof(want)), "the bad packet gets NAK");
}

static void t_duplicate(void)
{
	header("d.bin", 300);
	packet(1, pattern, 128, 0);
	packet(1, pattern, 128, 0);
	packet(2, pattern + 128, 128, 0);
	packet(3, pattern + 256, 44, 0);
	end_of_file();
	end_of_batch();
	openplc_ymodem_result_t r = openplcYmodemReceive(&IO, 3000);
	check(r == OPENPLC_YMODEM_DONE && file_len == 300 && memcmp(file, pattern, 300) == 0,
	      "a repeated packet is acknowledged and not written twice");
}

static void t_cancel(void)
{
	header("x.bin", 2000);
	packet(1, pattern, 1024, 0);
	put(CAN);
	put(CAN);
	openplc_ymodem_result_t r = openplcYmodemReceive(&IO, 3000);
	check(r == OPENPLC_YMODEM_CANCELLED, "two CANs from the sender cancel");
	check(closed == 1 && !closed_ok, "the file is closed as not complete");
}

static void t_idle(void)
{
	openplc_ymodem_result_t r = openplcYmodemReceive(&IO, 3000);
	static const uint8_t want[] = { 'C', 'C', 'C' };
	check(r == OPENPLC_YMODEM_IDLE && opened == 0, "no sender within the window is IDLE");
	check(sent_is(want, sizeof(want)), "it asks once a second for the window");
}

static void t_noise(void)
{
	const char *line = "[NET] ip=10.32.2.50\r\n";
	for (const char *p = line; *p; p++) {
		put((uint8_t)*p);
	}
	put(-1);
	header("n.bin", 10);
	packet(1, pattern, 10, 0);
	end_of_file();
	end_of_batch();
	openplc_ymodem_result_t r = openplcYmodemReceive(&IO, 3000);
	check(r == OPENPLC_YMODEM_DONE && file_len == 10 && memcmp(file, pattern, 10) == 0,
	      "text before the first packet is skipped");
}

static void t_refuse(void)
{
	refuse_open = 1;
	header("r.bin", 10);
	openplc_ymodem_result_t r = openplcYmodemReceive(&IO, 3000);
	check(r == OPENPLC_YMODEM_CANCELLED && nsent >= 3 && sent[nsent - 1] == CAN && sent[nsent - 2] == CAN,
	      "a file the sketch refuses is cancelled with two CANs");
}

static void t_short_file(void)
{
	header("t.bin", 500);
	packet(1, pattern, 128, 0);
	end_of_file();
	end_of_batch();
	openplcYmodemReceive(&IO, 3000);
	check(closed == 1 && !closed_ok && file_len == 128, "a file shorter than announced is closed as not complete");
}

int main(int argc, char **argv)
{
	const char *s = (argc > 1) ? argv[1] : "";
	reset();
	if      (strcmp(s, "one_file") == 0)      { t_one_file(); }
	else if (strcmp(s, "short_packets") == 0) { t_short_packets(); }
	else if (strcmp(s, "bad_crc") == 0)       { t_bad_crc(); }
	else if (strcmp(s, "duplicate") == 0)     { t_duplicate(); }
	else if (strcmp(s, "cancel") == 0)        { t_cancel(); }
	else if (strcmp(s, "idle") == 0)          { t_idle(); }
	else if (strcmp(s, "noise") == 0)         { t_noise(); }
	else if (strcmp(s, "refuse") == 0)        { t_refuse(); }
	else if (strcmp(s, "short_file") == 0)    { t_short_file(); }
	else {
		printf("unknown scenario \"%s\"\n", s);
		return 2;
	}
	return failures ? 1 : 0;
}
