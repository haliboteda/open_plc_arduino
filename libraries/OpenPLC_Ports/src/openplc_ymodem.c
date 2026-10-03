/*
 * openplc_ymodem.c -- see openplc_ymodem.h.
 */
#include "openplc_ymodem.h"

#include <stddef.h>

#define SOH 0x01U
#define STX 0x02U
#define EOT 0x04U
#define ACK 0x06U
#define NAK 0x15U
#define CAN 0x18U
#define CRC_REQUEST 'C'

#define BYTE_TIMEOUT_MS   1000U
#define PACKET_TIMEOUT_MS 3000U
#define PURGE_QUIET_MS     100U
#define MAX_ERRORS          10U

#define PKT_EOT    0
#define PKT_BAD   (-1)
#define PKT_CAN   (-2)

static uint8_t s_buf[1024];

static uint16_t crc16(const uint8_t *p, uint32_t n)
{
    uint16_t c = 0U;
    while (n-- > 0U) {
        c ^= (uint16_t)((uint16_t)*p++ << 8);
        for (int i = 0; i < 8; i++) {
            c = (c & 0x8000U) ? (uint16_t)((c << 1) ^ 0x1021U) : (uint16_t)(c << 1);
        }
    }
    return c;
}

/* The rest of a packet whose first byte is `first`: its data length, or one
 * of the PKT_ codes. */
static int read_packet(const openplc_ymodem_io_t *io, int first, uint8_t *seq)
{
    uint32_t size;
    int b;

    if (first == (int)EOT) {
        return PKT_EOT;
    }
    if (first == (int)CAN) {
        return (io->read(io->ctx, BYTE_TIMEOUT_MS) == (int)CAN) ? PKT_CAN : PKT_BAD;
    }
    if (first == (int)SOH) {
        size = 128U;
    } else if (first == (int)STX) {
        size = 1024U;
    } else {
        return PKT_BAD;
    }

    int s = io->read(io->ctx, BYTE_TIMEOUT_MS);
    int ns = io->read(io->ctx, BYTE_TIMEOUT_MS);
    if (s < 0 || ns < 0 || (uint8_t)s != (uint8_t)~(uint8_t)ns) {
        return PKT_BAD;
    }
    for (uint32_t i = 0U; i < size; i++) {
        if ((b = io->read(io->ctx, BYTE_TIMEOUT_MS)) < 0) {
            return PKT_BAD;
        }
        s_buf[i] = (uint8_t)b;
    }
    int hi = io->read(io->ctx, BYTE_TIMEOUT_MS);
    int lo = io->read(io->ctx, BYTE_TIMEOUT_MS);
    if (hi < 0 || lo < 0 || crc16(s_buf, size) != (uint16_t)((hi << 8) | lo)) {
        return PKT_BAD;
    }
    *seq = (uint8_t)s;
    return (int)size;
}

/* After a garbled packet, wait for the line to go quiet before answering. */
static void purge(const openplc_ymodem_io_t *io)
{
    while (io->read(io->ctx, PURGE_QUIET_MS) >= 0) {
    }
}

static void cancel(const openplc_ymodem_io_t *io)
{
    io->write(io->ctx, CAN);
    io->write(io->ctx, CAN);
}

static uint32_t parse_size(const uint8_t *p, const uint8_t *end)
{
    uint32_t v = 0U;
    while (p < end && *p >= '0' && *p <= '9') {
        v = v * 10U + (uint32_t)(*p++ - '0');
    }
    return v;
}

/* Waits for the packet that names the next file. Its length on success. */
static int read_header(const openplc_ymodem_io_t *io, uint32_t window_ms, bool first_file)
{
    uint32_t waited = 0U;
    unsigned errors = 0U;
    uint8_t seq;

    for (;;) {
        io->write(io->ctx, CRC_REQUEST);
        int b = io->read(io->ctx, BYTE_TIMEOUT_MS);
        if (b < 0) {
            waited += BYTE_TIMEOUT_MS;
            if (first_file ? (waited >= window_ms) : (++errors > MAX_ERRORS)) {
                return first_file ? PKT_EOT : PKT_BAD;
            }
            continue;
        }
        int n = read_packet(io, b, &seq);
        if (n == PKT_CAN) {
            return PKT_CAN;
        }
        if (n > 0 && seq == 0U) {
            return n;
        }
        purge(io);
        if (++errors > MAX_ERRORS) {
            return PKT_BAD;
        }
    }
}

/* The data packets of one file, through its two EOTs. */
static openplc_ymodem_result_t read_file(const openplc_ymodem_io_t *io, uint32_t size)
{
    uint32_t remaining = size;
    uint8_t expected = 1U;
    unsigned errors = 0U;
    bool eot_seen = false;
    uint8_t seq;

    for (;;) {
        int b = io->read(io->ctx, PACKET_TIMEOUT_MS);
        int n = (b < 0) ? PKT_BAD : read_packet(io, b, &seq);

        if (n == PKT_CAN) {
            io->close(io->ctx, false);
            return OPENPLC_YMODEM_CANCELLED;
        }
        if (n == PKT_EOT) {
            if (!eot_seen) {
                eot_seen = true;
                io->write(io->ctx, NAK);
                continue;
            }
            io->write(io->ctx, ACK);
            io->close(io->ctx, remaining == 0U);
            return OPENPLC_YMODEM_DONE;
        }
        if (n == PKT_BAD) {
            if (b >= 0) {
                purge(io);
            }
            if (++errors > MAX_ERRORS) {
                cancel(io);
                io->close(io->ctx, false);
                return OPENPLC_YMODEM_FAILED;
            }
            io->write(io->ctx, NAK);
            continue;
        }
        if (seq == (uint8_t)(expected - 1U)) {
            io->write(io->ctx, ACK);   /* our ACK was lost; already written */
            continue;
        }
        if (seq != expected) {
            cancel(io);
            io->close(io->ctx, false);
            return OPENPLC_YMODEM_FAILED;
        }
        uint32_t len = ((uint32_t)n < remaining) ? (uint32_t)n : remaining;
        if (len > 0U && !io->data(io->ctx, s_buf, len)) {
            cancel(io);
            io->close(io->ctx, false);
            return OPENPLC_YMODEM_CANCELLED;
        }
        remaining -= len;
        expected++;
        errors = 0U;
        io->write(io->ctx, ACK);
    }
}

openplc_ymodem_result_t openplcYmodemReceive(const openplc_ymodem_io_t *io,
                                             uint32_t start_window_ms)
{
    bool first = true;

    for (;;) {
        int n = read_header(io, start_window_ms, first);
        if (n == PKT_CAN) {
            return OPENPLC_YMODEM_CANCELLED;
        }
        if (n == PKT_EOT) {
            return OPENPLC_YMODEM_IDLE;
        }
        if (n < 0) {
            cancel(io);
            return OPENPLC_YMODEM_FAILED;
        }
        if (s_buf[0] == 0U) {   /* an empty name ends the batch */
            io->write(io->ctx, ACK);
            return OPENPLC_YMODEM_DONE;
        }

        char name[OPENPLC_YMODEM_NAME_MAX];
        size_t i = 0U;
        while (i + 1U < sizeof(name) && i < (size_t)n && s_buf[i] != 0U) {
            name[i] = (char)s_buf[i];
            i++;
        }
        name[i] = '\0';
        while (i < (size_t)n && s_buf[i] != 0U) {
            i++;   /* a name longer than the buffer is cut, not mis-parsed */
        }
        uint32_t size = (i + 1U < (size_t)n) ? parse_size(&s_buf[i + 1U], &s_buf[n]) : 0U;

        if (!io->open(io->ctx, name, size)) {
            cancel(io);
            return OPENPLC_YMODEM_CANCELLED;
        }
        io->write(io->ctx, ACK);
        io->write(io->ctx, CRC_REQUEST);
        first = false;

        openplc_ymodem_result_t r = read_file(io, size);
        if (r != OPENPLC_YMODEM_DONE) {
            return r;
        }
    }
}
