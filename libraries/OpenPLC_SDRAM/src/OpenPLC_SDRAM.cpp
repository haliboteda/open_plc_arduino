/*
 * See OpenPLC_SDRAM.h for why this is an API rather than a linker section.
 *
 * The controller setup, the power-up sequence and the pin map below are ported
 * from the bootloader's Core/Src/fmc.c in open_plc_cube_ide, which drives this
 * same chip on this same board. ⚠️ They are a second copy: change one, change
 * the other. The pin list is compared automatically by case P2
 * (IAPTranfer_Tool/TestTool/tools/check-mirror-sync.ps1, anchor "FMC pin map"),
 * but the timings below are NOT -- those still have to be kept in step by hand.
 */

#include "OpenPLC_SDRAM.h"
#include <string.h>

OpenPLC_SDRAM_Class SDRAM;

#if defined(HAL_SDRAM_MODULE_ENABLED)

static SDRAM_HandleTypeDef hsdram1;

/*
 * FMC pin configuration. Every pin is AF12, push-pull, no pull, very high
 * speed -- an SDRAM bus at 100 MHz will not tolerate slower slew settings.
 *
 * Grouped by port exactly as the bootloader does, so the two can be diffed.
 */
static void sdram_gpio_init(void)
{
  GPIO_InitTypeDef g = {0};
  g.Mode = GPIO_MODE_AF_PP;
  g.Pull = GPIO_NOPULL;
  g.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  g.Alternate = GPIO_AF12_FMC;

  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();

  /* PE0/1 = NBL0/1, PE7..PE15 = D4..D12 */
  g.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9 |
          GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 |
          GPIO_PIN_15;
  HAL_GPIO_Init(GPIOE, &g);

  /* PG0/1/2 = A10..A12, PG4/5 = BA0/1, PG8 = SDCLK, PG15 = SDNCAS */
  g.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_4 | GPIO_PIN_5 |
          GPIO_PIN_8 | GPIO_PIN_15;
  HAL_GPIO_Init(GPIOG, &g);

  /* PD0/1 = D2/D3, PD8/9/10 = D13..D15, PD14/15 = D0/D1 */
  g.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 |
          GPIO_PIN_14 | GPIO_PIN_15;
  HAL_GPIO_Init(GPIOD, &g);

  /* PF0..PF5 = A0..A5, PF11 = SDNRAS, PF12..PF15 = A6..A9 */
  g.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4 |
          GPIO_PIN_5 | GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 |
          GPIO_PIN_15;
  HAL_GPIO_Init(GPIOF, &g);

  /* PH2 = SDCKE0, PH3 = SDNE0 */
  g.Pin = GPIO_PIN_2 | GPIO_PIN_3;
  HAL_GPIO_Init(GPIOH, &g);

  /* PC0 = SDNWE */
  g.Pin = GPIO_PIN_0;
  HAL_GPIO_Init(GPIOC, &g);
}

/*
 * The SDRAM chip's own power-up sequence.
 *
 * HAL_SDRAM_Init() programs the FMC controller only. Until the commands below
 * have run, the chip has no mode register and no auto-refresh: reads and writes
 * return garbage and nothing is retained, and neither the HAL nor the hardware
 * says a word about it. CubeMX does not generate this.
 *
 * Values are the hardware engineer's, carried over verbatim from the
 * bootloader. Nothing here was derived or guessed.
 */
static void sdram_power_up(void)
{
  FMC_SDRAM_CommandTypeDef cmd;

  cmd.CommandMode = FMC_SDRAM_CMD_CLK_ENABLE;
  cmd.CommandTarget = FMC_SDRAM_CMD_TARGET_BANK1;
  cmd.AutoRefreshNumber = 1;
  cmd.ModeRegisterDefinition = 0;
  HAL_SDRAM_SendCommand(&hsdram1, &cmd, 0xFFFU);
  HAL_Delay(1); /* datasheet wants >=100 us; HAL_Delay granularity is 1 ms */

  cmd.CommandMode = FMC_SDRAM_CMD_PALL;
  HAL_SDRAM_SendCommand(&hsdram1, &cmd, 0xFFFU);

  cmd.CommandMode = FMC_SDRAM_CMD_AUTOREFRESH_MODE;
  cmd.AutoRefreshNumber = 2;
  HAL_SDRAM_SendCommand(&hsdram1, &cmd, 0xFFFU);

  /* Burst length 1, sequential, CAS latency 2, standard mode, single write. */
  cmd.CommandMode = FMC_SDRAM_CMD_LOAD_MODE;
  cmd.ModeRegisterDefinition =
      (uint32_t)0U | (0U << 3) | (2U << 4) | (0U << 7) | (1U << 9);
  HAL_SDRAM_SendCommand(&hsdram1, &cmd, 0xFFFU);

  /* COUNT = (64 ms self-refresh / 8192 rows) * 100 MHz SDCLK - 20 ~= 762.
     SDCLK is D1HCLK/2, which is what SDClockPeriod = 2 selects. */
  HAL_SDRAM_ProgramRefreshRate(&hsdram1, 762);
}

/*
 * Read-back over a handful of words spread across the whole 64 MB.
 *
 * Deliberately not a full scan: what this catches is a controller that was
 * never configured and a power-up sequence that went missing (both fail on the
 * first access), plus stuck address lines, which show up as two offsets
 * aliasing onto one cell. Marginal cells and a wrong refresh rate need a soak
 * test, not a boot check.
 *
 * Every write happens before any read, for the aliasing reason: if two offsets
 * land on the same cell, the second write destroys the first value and the
 * read-back disagrees.
 */
bool OpenPLC_SDRAM_Class::selfTest()
{
  static const uint32_t offsets[] = {
    0x0000000U, 0x0000004U, 0x0001000U, 0x0040000U,
    0x0100000U, 0x1000000U, CAPACITY - 4U
  };
  volatile uint32_t *ram = (volatile uint32_t *)BASE;
  const uint32_t n = (uint32_t)(sizeof(offsets) / sizeof(offsets[0]));

  for (uint32_t i = 0; i < n; i++) {
    ram[offsets[i] / 4U] = 0xA5A5A5A5U ^ offsets[i];
  }
  for (uint32_t i = 0; i < n; i++) {
    if (ram[offsets[i] / 4U] != (0xA5A5A5A5U ^ offsets[i])) {
      return false;
    }
  }
  return true;
}

bool OpenPLC_SDRAM_Class::begin()
{
  if (_ready) {
    return true;
  }

  RCC_PeriphCLKInitTypeDef clk = {0};
  clk.PeriphClockSelection = RCC_PERIPHCLK_FMC;
  clk.FmcClockSelection = RCC_FMCCLKSOURCE_D1HCLK;
  if (HAL_RCCEx_PeriphCLKConfig(&clk) != HAL_OK) {
    return false;
  }
  __HAL_RCC_FMC_CLK_ENABLE();
  sdram_gpio_init();

  FMC_SDRAM_TimingTypeDef t = {0};
  hsdram1.Instance = FMC_SDRAM_DEVICE;
  hsdram1.Init.SDBank = FMC_SDRAM_BANK1;
  hsdram1.Init.ColumnBitsNumber = FMC_SDRAM_COLUMN_BITS_NUM_10;
  hsdram1.Init.RowBitsNumber = FMC_SDRAM_ROW_BITS_NUM_13;
  hsdram1.Init.MemoryDataWidth = FMC_SDRAM_MEM_BUS_WIDTH_16;
  hsdram1.Init.InternalBankNumber = FMC_SDRAM_INTERN_BANKS_NUM_4;
  hsdram1.Init.CASLatency = FMC_SDRAM_CAS_LATENCY_2;
  hsdram1.Init.WriteProtection = FMC_SDRAM_WRITE_PROTECTION_DISABLE;
  hsdram1.Init.SDClockPeriod = FMC_SDRAM_CLOCK_PERIOD_2;
  hsdram1.Init.ReadBurst = FMC_SDRAM_RBURST_ENABLE;
  hsdram1.Init.ReadPipeDelay = FMC_SDRAM_RPIPE_DELAY_2;
  t.LoadToActiveDelay = 2;
  t.ExitSelfRefreshDelay = 7;
  t.SelfRefreshTime = 5;
  t.RowCycleDelay = 7;
  t.WriteRecoveryTime = 2;
  t.RPDelay = 3;
  t.RCDDelay = 3;

  if (HAL_SDRAM_Init(&hsdram1, &t) != HAL_OK) {
    return false;
  }
  sdram_power_up();

  if (!selfTest()) {
    return false;
  }

  _ready = true;
  _used = 0;
  return true;
}

void *OpenPLC_SDRAM_Class::allocUninitialized(size_t bytes, size_t align)
{
  if (!_ready || bytes == 0) {
    return nullptr;
  }
  if (align == 0) {
    align = 1;
  }
  size_t start = (_used + (align - 1)) & ~(align - 1);
  /* Written so an absurd `bytes` cannot wrap the addition and pass the check. */
  if (start > CAPACITY || bytes > CAPACITY - start) {
    return nullptr;
  }
  _used = start + bytes;
  return (void *)(BASE + start);
}

void *OpenPLC_SDRAM_Class::alloc(size_t bytes, size_t align)
{
  void *p = allocUninitialized(bytes, align);
  if (p != nullptr) {
    memset(p, 0, bytes);
  }
  return p;
}

#else /* !HAL_SDRAM_MODULE_ENABLED */

/*
 * Built without the SDRAM HAL module. Fail closed and loudly at the API, rather
 * than silently returning addresses to a controller that was never started.
 */
bool OpenPLC_SDRAM_Class::selfTest() { return false; }
bool OpenPLC_SDRAM_Class::begin() { return false; }
void *OpenPLC_SDRAM_Class::allocUninitialized(size_t, size_t) { return nullptr; }
void *OpenPLC_SDRAM_Class::alloc(size_t, size_t) { return nullptr; }

#endif
