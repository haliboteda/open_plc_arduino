/*
 * T3-09: the KNX TP1 data link engine (OpenPLC_KNX stknx_tp1.c).
 * Drives the real engine with a simulated bus: the timer's bit periods, the
 * one-period output pipeline, wired-AND pulses from a scripted peer and the
 * engine's own pulses coming back through the receiver.
 * Rules under test: $PROD/docs/modules/M3/KNX-TP-DATA-LINK.md
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "stknx_tp1.h"

#define BIT       STKNX_BIT_US
#define LAT       2u     /* bit-period interrupt reads the clock this late */
#define OPTO      2u     /* a pulse reaches the capture input this late */
#define MAXP      20000

static int failures;

static void check(int ok, const char *what)
{
	printf("[%s] %s\n", ok ? "PASS" : "FAIL", what);
	if (!ok) {
		failures++;
	}
}

/* --- Simulated bus ---------------------------------------------------- */

static stknx_link_t L;
static uint32_t now;            /* us */
static uint32_t next_tick;
static uint16_t len_cur;
static uint8_t  hw_pre;         /* the timer's preload: next period's pulse */

static uint32_t peer[MAXP];     /* scheduled peer pulse starts, sorted */
static int      npeer, ipeer;

static uint32_t bus[MAXP];      /* every falling edge the receiver saw */
static int      nbus;
static uint32_t dut[MAXP];      /* the pulses the engine put out */
static int      ndut;
static uint32_t last_low = 0xFFFF0000u;

static uint16_t accept_ga = 0x0801;   /* 1/0/1 */
static uint8_t  accept_all_ia;

static uint8_t addressed(void *ctx, uint16_t dst, uint8_t is_group)
{
	(void)ctx;
	return is_group ? (uint8_t)(dst == accept_ga) : accept_all_ia;
}

static void bus_pulse(uint32_t t, int from_dut)
{
	if (from_dut) {
		dut[ndut++] = t;
	}
	/* Wired-AND: a pulse that starts while the line is already active makes
	 * no new edge. */
	if ((t - last_low) < STKNX_PULSE_US) {
		return;
	}
	last_low = t;
	bus[nbus++] = t;
	if (stknx_link_pulse(&L, (uint16_t)(t + OPTO))) {
		hw_pre = 0;
	}
}

static void sim_reset(void)
{
	stknx_link_init(&L, addressed, NULL);
	now = 1000;
	next_tick = now;
	len_cur = BIT;
	hw_pre = 0;
	npeer = ipeer = nbus = ndut = 0;
	last_low = 0xFFFF0000u;
	accept_ga = 0x0801;
	accept_all_ia = 0;
}

static void run_until(uint32_t t_end)
{
	while (1) {
		uint32_t tp = (ipeer < npeer) ? peer[ipeer] : 0xFFFFFFFFu;
		if ((next_tick > t_end) && (tp > t_end)) {
			break;
		}
		if (next_tick <= tp) {
			uint16_t len = BIT;
			uint8_t emit = hw_pre;   /* update event: preload becomes active */
			now = next_tick;
			hw_pre = stknx_link_tick(&L, (uint16_t)(now + LAT), &len);
			if (emit) {
				bus_pulse(now, 1);
			}
			next_tick = now + len_cur;
			len_cur = len;
		} else {
			now = tp;
			ipeer++;
			bus_pulse(tp, 0);
		}
	}
	now = t_end;
}

static void peer_char(uint32_t t0, uint8_t b)
{
	uint16_t m = stknx_encode_char(b);
	for (int i = 0; i < 11; i++) {
		uint32_t t = t0 + (uint32_t)i * BIT;
		/* Already past: it coincides with one of the engine's pulses, which
		 * made the edge. */
		if ((m & (1u << i)) && (t > now)) {
			int k = npeer++;
			while ((k > ipeer) && (peer[k - 1] > t)) { peer[k] = peer[k - 1]; k--; }
			peer[k] = t;
		}
	}
}

/* Returns the start time just past the frame's last character. */
static uint32_t peer_frame(uint32_t t0, const uint8_t *f, int n)
{
	for (int i = 0; i < n; i++) {
		peer_char(t0 + (uint32_t)i * STKNX_CHAR_SLOTS * BIT, f[i]);
	}
	return t0 + (uint32_t)(n - 1) * STKNX_CHAR_SLOTS * BIT;
}

/* Decodes characters out of a sorted pulse list, independently of the engine. */
static int decode(const uint32_t *p, int np, uint8_t *out, uint32_t *t0s, int cap)
{
	int n = 0, i = 0;
	while ((i < np) && (n < cap)) {
		uint32_t t0 = p[i];
		uint16_t m = 0;
		while ((i < np) && (((p[i] - t0) + BIT / 2) / BIT <= 10)) {
			m |= (uint16_t)(1u << (((p[i] - t0) + BIT / 2) / BIT));
			i++;
		}
		uint8_t b;
		if (!stknx_decode_char(m, &b)) {
			return -1;
		}
		t0s[n] = t0;
		out[n++] = b;
	}
	return n;
}

/* 1/0/1 GroupValueWrite from 1.1.20; the octets ETS would put on the bus. */
static int group_write(uint8_t *f, uint16_t ga, uint8_t val)
{
	f[0] = 0xBC; f[1] = 0x11; f[2] = 0x14;
	f[3] = (uint8_t)(ga >> 8); f[4] = (uint8_t)ga;
	f[5] = 0xE1; f[6] = 0x00; f[7] = (uint8_t)(0x80 | val);
	f[8] = stknx_checksum(f, 8);
	return 9;
}

/* --- Scenarios ------------------------------------------------------------- */

static void t_codec(void)
{
	int all = 1;
	for (int b = 0; b < 256; b++) {
		uint8_t got;
		if (!stknx_decode_char(stknx_encode_char((uint8_t)b), &got) || (got != b)) { all = 0; }
	}
	check(all, "every octet survives encode -> decode");

	uint8_t got;
	uint16_t m = stknx_encode_char(0x55);
	check(!stknx_decode_char(m ^ (1u << 9), &got), "a flipped parity bit is rejected");
	check(!stknx_decode_char(m & ~1u, &got), "a missing start bit is rejected");
	check(!stknx_decode_char(m | (1u << 10), &got), "a pulse in the stop bit is rejected");
	check(stknx_encode_char(0xFF) == 0x0201, "0xFF is the start bit and a parity pulse (eight ones, parity bit 0)");

	/* GroupValueWrite 1 from 15.15.250 to 1/0/1: XOR of the eight octets is 0xD0. */
	static const uint8_t fx[] = { 0xBC, 0xFF, 0xFA, 0x08, 0x01, 0xE1, 0x00, 0x81, 0x2F };
	check(stknx_checksum(fx, 8) == 0x2F, "check octet is the inverted XOR of the octets before it");
	check(stknx_frame_length(fx, 5) == 0 && stknx_frame_length(fx, 6) == 9, "standard frame length known at octet 6");
	static const uint8_t ex[] = { 0x10, 0xE0, 0x11, 0x14, 0x08, 0x01, 0x20 };
	check(stknx_frame_length(ex, 6) == 0 && stknx_frame_length(ex, 7) == 9 + 0x20, "extended frame length from octet 7");
	check(stknx_ack_kind(STKNX_ACK) == STKNX_ACKKIND_ACK && stknx_ack_kind(STKNX_NAK) == STKNX_ACKKIND_NAK
	      && stknx_ack_kind(STKNX_BUSY) == STKNX_ACKKIND_BUSY && stknx_ack_kind(0xBC) == STKNX_ACKKIND_NONE,
	      "acknowledge octets told apart from a control field");
}

/* A peer frame arrives; returns the engine's pulses decoded, if any. */
static int rx_case(uint16_t ga, int corrupt, uint8_t *reply, uint32_t *reply_t0, uint32_t *end_t0)
{
	uint8_t f[16];
	int n = group_write(f, ga, 1);
	if (corrupt) { f[8] ^= 0x01; }
	run_until(now + 60 * BIT);
	*end_t0 = peer_frame(now + 10, f, n);
	run_until(*end_t0 + 60 * BIT);
	uint8_t out[4];
	uint32_t t0s[4];
	int k = decode(dut, ndut, out, t0s, 4);
	if (k == 1) { *reply = out[0]; *reply_t0 = t0s[0]; }
	return k;
}

static void t_rx_ack(void)
{
	uint8_t r = 0, buf[STKNX_FRAME_MAX];
	uint32_t rt = 0, end = 0;
	int k = rx_case(0x0801, 0, &r, &rt, &end);
	check(k == 1 && r == STKNX_ACK, "an addressed frame is answered with ACK");
	uint32_t want = end + (11 + STKNX_ACK_GAP_BITS) * BIT;
	check(k == 1 && rt >= want - 10 && rt <= want + 10, "the ACK starts 15 bit periods after the stop bit (+-10 us)");
	uint16_t n = stknx_link_receive(&L, buf, sizeof(buf));
	check(n == 9 && buf[3] == 0x08 && buf[4] == 0x01, "the frame reaches the main loop with its octets");
	check(stknx_link_receive(&L, buf, sizeof(buf)) == 0, "and only once");
}

static void t_rx_other(void)
{
	uint8_t r = 0, buf[STKNX_FRAME_MAX];
	uint32_t rt = 0, end = 0;
	int k = rx_case(0x0802, 0, &r, &rt, &end);
	check(k == 0 && ndut == 0, "a frame for somebody else gets no acknowledge");
	check(stknx_link_receive(&L, buf, sizeof(buf)) == 0, "and is not handed up");
}

static void t_rx_nak(void)
{
	uint8_t r = 0, buf[STKNX_FRAME_MAX];
	uint32_t rt = 0, end = 0;
	int k = rx_case(0x0801, 1, &r, &rt, &end);
	check(k == 1 && r == STKNX_NAK, "an addressed frame with a bad check octet is answered with NAK");
	check(stknx_link_receive(&L, buf, sizeof(buf)) == 0, "and is not handed up");
}

static void t_rx_busy(void)
{
	uint8_t r = 0;
	uint32_t rt = 0, end = 0;
	for (int i = 0; i < (int)STKNX_RX_QUEUE - 1; i++) {
		ndut = 0;
		(void)rx_case(0x0801, 0, &r, &rt, &end);
	}
	ndut = 0;
	int k = rx_case(0x0801, 0, &r, &rt, &end);
	check(k == 1 && r == STKNX_BUSY, "a full receive queue answers BUSY");
}

/* Starts a send; returns the time of the engine's first pulse. */
static uint32_t tx_start(uint8_t *f, int *n)
{
	*n = group_write(f, 0x0801, 1);
	stknx_link_send(&L, f, (uint16_t)*n);
	run_until(now + 80 * BIT);
	return ndut ? dut[0] : 0;
}

static int tx_frames(uint8_t frames[][16], uint32_t *starts, int cap)
{
	uint8_t out[128];
	uint32_t t0s[128];
	int k = decode(dut, ndut, out, t0s, 128), nf = 0;
	for (int i = 0; (i + 9 <= k) && (nf < cap); i += 9) {
		memcpy(frames[nf], &out[i], 9);
		starts[nf++] = t0s[i];
	}
	return (k % 9 == 0) ? nf : -1;
}

static void t_tx_ack(void)
{
	uint8_t f[16], got[4][16];
	uint32_t st[4];
	int n;
	uint32_t t0 = tx_start(f, &n);
	check(t0 >= 1000 + STKNX_IDLE_BEFORE_TX_BITS * BIT, "nothing is sent before the bus has been idle 50 bit periods");
	uint32_t end = t0 + (uint32_t)(n - 1) * STKNX_CHAR_SLOTS * BIT;
	peer_char(end + (11 + STKNX_ACK_GAP_BITS) * BIT, STKNX_ACK);
	run_until(end + 400 * BIT);
	int nf = tx_frames(got, st, 4);
	check(nf == 1 && memcmp(got[0], f, 9) == 0, "the frame goes out once, octets as given, 13 bit periods apart");
	check(stknx_link_send_result(&L) == STKNX_SEND_OK, "the ACK confirms the send");
	uint8_t buf[STKNX_FRAME_MAX];
	check(stknx_link_receive(&L, buf, sizeof(buf)) == 0, "our own frame is not handed up as received");
}

static void t_tx_noack(void)
{
	uint8_t f[16], got[8][16];
	uint32_t st[8];
	int n;
	(void)tx_start(f, &n);
	run_until(now + 2000 * BIT);
	int nf = tx_frames(got, st, 8);
	check(nf == 1 + STKNX_REPEATS, "without an acknowledge the frame is repeated 3 times");
	int rep_ok = (nf > 1);
	for (int i = 1; i < nf; i++) {
		rep_ok = rep_ok && ((got[i][0] & 0x20) == 0) && (got[i][8] == stknx_checksum(got[i], 8))
		       && (memcmp(&got[i][1], &f[1], 7) == 0);
	}
	check(rep_ok, "repeats clear the not-repeated bit and carry a fresh check octet");
	int gaps_ok = (nf > 1);
	for (int i = 1; i < nf; i++) {
		gaps_ok = gaps_ok && ((st[i] - (st[i - 1] + 8 * STKNX_CHAR_SLOTS * BIT + 11 * BIT))
		                      >= STKNX_IDLE_BEFORE_TX_BITS * BIT);
	}
	check(gaps_ok, "each repeat waits for 50 idle bit periods");
	check(stknx_link_send_result(&L) == STKNX_SEND_FAILED, "and the send is reported failed");
}

static void t_tx_busy(void)
{
	uint8_t f[16], got[4][16];
	uint32_t st[4];
	int n;
	uint32_t t0 = tx_start(f, &n);
	uint32_t end = t0 + (uint32_t)(n - 1) * STKNX_CHAR_SLOTS * BIT;
	peer_char(end + (11 + STKNX_ACK_GAP_BITS) * BIT, STKNX_BUSY);
	uint32_t busy_end = end + (11 + STKNX_ACK_GAP_BITS + 11) * BIT;
	int first = ndut;
	run_until(busy_end);
	first = ndut;
	while ((ndut == first) && (now < busy_end + 400 * BIT)) { run_until(now + BIT / 4); }
	uint32_t t1 = (ndut > first) ? dut[first] : 0;
	check(t1 != 0 && (t1 - busy_end) >= STKNX_IDLE_AFTER_BUSY_BITS * BIT,
	      "after BUSY the repeat waits 150 idle bit periods");
	uint32_t end2 = t1 + (uint32_t)(n - 1) * STKNX_CHAR_SLOTS * BIT;
	peer_char(end2 + (11 + STKNX_ACK_GAP_BITS) * BIT, STKNX_ACK);
	run_until(end2 + 100 * BIT);
	int nf = tx_frames(got, st, 4);
	check(nf == 2 && (got[1][0] & 0x20) == 0, "exactly one repeat, marked as repeated");
	check(stknx_link_send_result(&L) == STKNX_SEND_OK, "and an ACK to the repeat confirms the send");
}

static void t_collision(void)
{
	/* Both start together. Octet 0 is the same; octet 1 (source high byte)
	 * is 0x11 for us and 0x10 for the peer, so in data bit 0 the peer sends
	 * a 0 (pulse) where we send a 1. */
	uint8_t f[16], pf[16];
	int n = group_write(f, 0x0801, 1);
	(void)group_write(pf, 0x0801, 0);
	pf[1] = 0x10;
	pf[8] = stknx_checksum(pf, 8);
	accept_ga = 0x0801;
	stknx_link_send(&L, f, (uint16_t)n);
	run_until(now + 60 * BIT);
	/* Find when the engine will start: run tick by tick until its first pulse. */
	while (ndut == 0) { run_until(now + BIT / 4); }
	uint32_t t0 = dut[0];
	uint32_t pend = peer_frame(t0, pf, 9);
	run_until(pend + 20 * BIT);

	uint32_t lost_slot = t0 + (STKNX_CHAR_SLOTS + 1) * BIT;   /* octet 1, data bit 0 */
	int extra = 0;
	for (int i = 0; i < ndut; i++) {
		if (dut[i] > lost_slot + BIT / 2) { extra++; }
	}
	check(extra == 0, "after losing arbitration the engine puts no further pulse on the bus");
	uint8_t buf[STKNX_FRAME_MAX];
	uint16_t r = stknx_link_receive(&L, buf, sizeof(buf));
	check(r == 9 && memcmp(buf, pf, 9) == 0, "the winner's frame is received intact");

	/* It owes the winner an ACK, then retries after the bus is idle. */
	int before = ndut;
	run_until(pend + 230 * BIT);
	uint8_t out[32];
	uint32_t t0s[32];
	int k = decode(&dut[before], ndut - before, out, t0s, 32);
	check(k >= 1 && out[0] == STKNX_ACK, "it acknowledges the winner's frame");
	check(k >= 10 && memcmp(&out[1], f, 9) == 0, "and sends its own frame again, unchanged");
}

int main(int argc, char **argv)
{
	const char *s = (argc > 1) ? argv[1] : "";
	sim_reset();

	if      (strcmp(s, "codec") == 0)     { t_codec(); }
	else if (strcmp(s, "rx_ack") == 0)    { t_rx_ack(); }
	else if (strcmp(s, "rx_other") == 0)  { t_rx_other(); }
	else if (strcmp(s, "rx_nak") == 0)    { t_rx_nak(); }
	else if (strcmp(s, "rx_busy") == 0)   { t_rx_busy(); }
	else if (strcmp(s, "tx_ack") == 0)    { t_tx_ack(); }
	else if (strcmp(s, "tx_noack") == 0)  { t_tx_noack(); }
	else if (strcmp(s, "tx_busy") == 0)   { t_tx_busy(); }
	else if (strcmp(s, "collision") == 0) { t_collision(); }
	else {
		printf("unknown scenario \"%s\"\n", s);
		return 2;
	}
	return failures ? 1 : 0;
}
