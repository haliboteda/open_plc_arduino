/*
 * OpenPLC_CAN -- classic CAN on FDCAN1 (CAN_TX_Pin / CAN_RX_Pin), polled.
 *
 *     CAN.begin(500000);
 *     CAN.write(0x123, data, 8);
 *     if (CAN.read(id, data, len)) { ... }
 *
 * Standard 11-bit IDs, up to 8 data bytes, every frame on the bus is received
 * (no ID filter). No interrupt handler and no HAL_FDCAN_MspInit are defined, so
 * an application stays free to use those; handle() gives the HAL handle for
 * anything this wrapper does not cover (filters, CAN FD).
 */

#ifndef OPENPLC_CAN_H_
#define OPENPLC_CAN_H_

#include <Arduino.h>

#if !defined(HAL_FDCAN_MODULE_ENABLED)
#error "OpenPLC_CAN needs HAL_FDCAN_MODULE_ENABLED (the board variant sets it unless HAL_FDCAN_MODULE_DISABLED is defined)"
#endif

class OpenPLC_CAN_Class {
  public:
    /* Supported bit rates: 125000, 250000, 500000, 1000000. Returns false for
     * any other rate or if the controller does not start. */
    bool begin(uint32_t bitrate);
    void end(void);

    /* Queues one data frame. Returns false if not started or the TX FIFO is full. */
    bool write(uint32_t id, const uint8_t *data, uint8_t len);

    /* Takes one received frame; data must hold 8 bytes. Returns false if none. */
    bool read(uint32_t &id, uint8_t *data, uint8_t &len);

    FDCAN_HandleTypeDef *handle(void);
};

extern OpenPLC_CAN_Class CAN;

#endif /* OPENPLC_CAN_H_ */
