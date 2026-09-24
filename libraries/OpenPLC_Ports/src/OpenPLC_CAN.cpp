/*
 * Ported from open_plc_cube_ide TestCase/common/port_can.c (runs on this
 * board): same timing table, FIFO sizes and "accept everything" filter.
 */

#include <Arduino.h>

#if defined(HAL_FDCAN_MODULE_ENABLED)

#include "OpenPLC_CAN.h"
#include "PeripheralPins.h"
#include <string.h>

OpenPLC_CAN_Class CAN;

/* Valid only with HSE (25 MHz) as the FDCAN kernel clock: 1 + seg1 + seg2 tq
 * per bit divides HSE exactly. */
struct can_timing_t {
  uint32_t bps;
  uint16_t prescaler;
  uint16_t seg1;
  uint16_t seg2;
  uint16_t sjw;
};

static const can_timing_t CAN_TIMINGS[] = {
  {  125000u, 1u, 174u, 25u, 4u },
  {  250000u, 1u,  87u, 12u, 4u },
  {  500000u, 1u,  43u,  6u, 4u },
  { 1000000u, 1u,  21u,  3u, 3u },
};

/* RX FIFO gives the sketch slack while it prints; HAL caps FIFO0 at 64. */
#define CAN_RX_FIFO_ELMTS 32u
#define CAN_TX_FIFO_ELMTS  8u

static FDCAN_HandleTypeDef s_h;
static bool s_open;

bool OpenPLC_CAN_Class::begin(uint32_t bitrate)
{
  const can_timing_t *t = NULL;
  for (size_t i = 0; i < sizeof(CAN_TIMINGS) / sizeof(CAN_TIMINGS[0]); i++) {
    if (CAN_TIMINGS[i].bps == bitrate) {
      t = &CAN_TIMINGS[i];
    }
  }
  if (t == NULL) {
    return false;
  }
  end();

  /* The Arduino clock setup runs on HSI and leaves HSE off; the timing table
   * needs HSE. See OpenPLC_Docs maps/arduino-examples-and-ide-flow/IDE-03-findings.md. */
  enableClock(HSE_CLOCK);
  RCC_PeriphCLKInitTypeDef p = {};
  p.PeriphClockSelection = RCC_PERIPHCLK_FDCAN;
  p.FdcanClockSelection = RCC_FDCANCLKSOURCE_HSE;
  if (HAL_RCCEx_PeriphCLKConfig(&p) != HAL_OK) {
    return false;
  }
  __HAL_RCC_FDCAN_CLK_ENABLE();

  pinmap_pinout(digitalPinToPinName(CAN_TX_Pin), PinMap_CAN_TD);
  pinmap_pinout(digitalPinToPinName(CAN_RX_Pin), PinMap_CAN_RD);

  s_h.Instance                  = FDCAN1;
  s_h.Init.FrameFormat          = FDCAN_FRAME_CLASSIC;
  s_h.Init.Mode                 = FDCAN_MODE_NORMAL;
  s_h.Init.AutoRetransmission   = ENABLE;
  s_h.Init.TransmitPause        = DISABLE;
  s_h.Init.ProtocolException    = DISABLE;
  s_h.Init.NominalPrescaler     = t->prescaler;
  s_h.Init.NominalSyncJumpWidth = t->sjw;
  s_h.Init.NominalTimeSeg1      = t->seg1;
  s_h.Init.NominalTimeSeg2      = t->seg2;
  s_h.Init.DataPrescaler        = 1u;   /* classic CAN: data phase unused */
  s_h.Init.DataSyncJumpWidth    = 1u;
  s_h.Init.DataTimeSeg1         = 1u;
  s_h.Init.DataTimeSeg2         = 1u;
  s_h.Init.MessageRAMOffset     = 0u;
  s_h.Init.StdFiltersNbr        = 0u;   /* the global filter takes everything */
  s_h.Init.ExtFiltersNbr        = 0u;
  s_h.Init.RxFifo0ElmtsNbr      = CAN_RX_FIFO_ELMTS;
  s_h.Init.RxFifo0ElmtSize      = FDCAN_DATA_BYTES_8;
  s_h.Init.RxFifo1ElmtsNbr      = 0u;
  s_h.Init.RxFifo1ElmtSize      = FDCAN_DATA_BYTES_8;
  s_h.Init.RxBuffersNbr         = 0u;
  s_h.Init.RxBufferSize         = FDCAN_DATA_BYTES_8;
  s_h.Init.TxEventsNbr          = 0u;
  s_h.Init.TxBuffersNbr         = 0u;
  s_h.Init.TxFifoQueueElmtsNbr  = CAN_TX_FIFO_ELMTS;
  s_h.Init.TxFifoQueueMode      = FDCAN_TX_FIFO_OPERATION;
  s_h.Init.TxElmtSize           = FDCAN_DATA_BYTES_8;

  if (HAL_FDCAN_Init(&s_h) != HAL_OK) {
    return false;
  }
  if (HAL_FDCAN_ConfigGlobalFilter(&s_h, FDCAN_ACCEPT_IN_RX_FIFO0,
                                   FDCAN_ACCEPT_IN_RX_FIFO0,
                                   FDCAN_FILTER_REMOTE,
                                   FDCAN_FILTER_REMOTE) != HAL_OK) {
    return false;
  }
  if (HAL_FDCAN_Start(&s_h) != HAL_OK) {
    return false;
  }
  s_open = true;
  return true;
}

void OpenPLC_CAN_Class::end(void)
{
  if (!s_open) {
    return;
  }
  (void)HAL_FDCAN_Stop(&s_h);
  (void)HAL_FDCAN_DeInit(&s_h);
  s_open = false;
}

bool OpenPLC_CAN_Class::write(uint32_t id, const uint8_t *data, uint8_t len)
{
  static const uint32_t DLC[9] = {
    FDCAN_DLC_BYTES_0, FDCAN_DLC_BYTES_1, FDCAN_DLC_BYTES_2,
    FDCAN_DLC_BYTES_3, FDCAN_DLC_BYTES_4, FDCAN_DLC_BYTES_5,
    FDCAN_DLC_BYTES_6, FDCAN_DLC_BYTES_7, FDCAN_DLC_BYTES_8
  };
  FDCAN_TxHeaderTypeDef tx = {};

  if (!s_open) {
    return false;
  }
  tx.Identifier          = id;
  tx.IdType              = FDCAN_STANDARD_ID;
  tx.TxFrameType         = FDCAN_DATA_FRAME;
  tx.DataLength          = DLC[(len > 8u) ? 8u : len];
  tx.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
  tx.BitRateSwitch       = FDCAN_BRS_OFF;
  tx.FDFormat            = FDCAN_CLASSIC_CAN;
  tx.TxEventFifoControl  = FDCAN_NO_TX_EVENTS;
  tx.MessageMarker       = 0u;

  /* HAL takes a non-const pointer but only reads it. */
  return HAL_FDCAN_AddMessageToTxFifoQ(&s_h, &tx, const_cast<uint8_t *>(data)) == HAL_OK;
}

bool OpenPLC_CAN_Class::read(uint32_t &id, uint8_t *data, uint8_t &len)
{
  FDCAN_RxHeaderTypeDef h;
  /* HAL copies DLCtoBytes[DLC] bytes, up to 64 for a classic DLC of 15. */
  uint8_t d[64];

  if (!s_open || HAL_FDCAN_GetRxFifoFillLevel(&s_h, FDCAN_RX_FIFO0) == 0u) {
    return false;
  }
  if (HAL_FDCAN_GetRxMessage(&s_h, FDCAN_RX_FIFO0, &h, d) != HAL_OK) {
    return false;
  }
  id = h.Identifier;
  /* This HAL keeps the DLC in bits 16..19; classic DLC 9..15 means 8 bytes. */
  len = (uint8_t)(h.DataLength >> 16);
  if (len > 8u) {
    len = 8u;
  }
  memcpy(data, d, len);
  return true;
}

FDCAN_HandleTypeDef *OpenPLC_CAN_Class::handle(void)
{
  return &s_h;
}

#endif /* HAL_FDCAN_MODULE_ENABLED */
