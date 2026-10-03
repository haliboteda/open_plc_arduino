/*
 * openplc_ymodem.h -- receive files with YMODEM over any byte stream.
 *
 * YMODEM because the first packet carries the file's name and exact length,
 * and common PC tools send it (Tera Term, lrzsz's sb).
 * $PROD/maps/core-examples-on-board/issues/EXB-03-how-does-a-file-get-from-rs232-onto-the-sd-card.md
 */
#ifndef OPENPLC_YMODEM_H_
#define OPENPLC_YMODEM_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OPENPLC_YMODEM_NAME_MAX 64U

typedef struct {
    void *ctx;
    /* One byte, or -1 if none arrives within timeout_ms. */
    int (*read)(void *ctx, uint32_t timeout_ms);
    void (*write)(void *ctx, uint8_t byte);
    /* A file starts. Returning false cancels the transfer. */
    bool (*open)(void *ctx, const char *name, uint32_t size);
    /* The next bytes of the file, already cut to its length. False cancels. */
    bool (*data)(void *ctx, const uint8_t *buf, uint32_t len);
    /* The file ended: ok is false if it was cancelled or came up short. */
    void (*close)(void *ctx, bool ok);
} openplc_ymodem_io_t;

typedef enum {
    OPENPLC_YMODEM_IDLE = 0,    /* no sender answered within the start window */
    OPENPLC_YMODEM_DONE,        /* the sender ended its batch */
    OPENPLC_YMODEM_CANCELLED,   /* the sender cancelled, or a callback refused */
    OPENPLC_YMODEM_FAILED       /* too many errors, or a packet out of order */
} openplc_ymodem_result_t;

/* Receives one batch of files. Asks for a sender for up to start_window_ms;
 * once one answers, blocks until the batch ends. */
openplc_ymodem_result_t openplcYmodemReceive(const openplc_ymodem_io_t *io,
                                             uint32_t start_window_ms);

#ifdef __cplusplus
}
#endif

#endif /* OPENPLC_YMODEM_H_ */
