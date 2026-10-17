#include "Arduino.h"
#include "openplc_syslog.h"
#include <string.h>

#if defined(USBCON) && defined(USBD_USE_CDC)
  #include "usbd_cdc_if.h"
#endif

/* Overridden by OPENPLC_SYSTEM_LOG() in the sketch. */
extern "C" __attribute__((weak)) const uint8_t openplc_system_log_on = 1;

static bool log_on(void)
{
  // volatile: the weak default must not be folded into this file.
  return *(const volatile uint8_t *)&openplc_system_log_on != 0U;
}

/* Sending to USB needs interrupts; from a handler or with them masked the
 * line only reaches the buffer and goes out on the next service call. */
static bool can_send_now(void)
{
  return (__get_IPSR() == 0U) && (__get_PRIMASK() == 0U);
}

extern "C" void openplc_syslog_write(const char *tag, const char *text, size_t len)
{
  char line[16 + BOOT_LOG_LINE_MAX];
  size_t n;

  if ((tag == NULL) || (text == NULL)) {
    return;
  }
  if ((len > 0U) && (text[0] == '[')) {
    tag = "[sys]";
  }
  n = strlen(tag);
  if (len > (sizeof(line) - n)) {
    len = sizeof(line) - n;
  }
  memcpy(line, tag, n);
  memcpy(line + n, text, len);
  (void)boot_log_write(line, n + len);
  if (can_send_now()) {
    openplc_syslog_service();
  }
}

/* Collects bytes into buf; a line goes out at '\n' or when buf is full. */
static void line_put(char *buf, size_t *len, const char *tag, char c)
{
  if (c == '\r') {
    return;
  }
  if (c != '\n') {
    buf[(*len)++] = c;
    if (*len < BOOT_LOG_LINE_MAX) {
      return;
    }
  }
  if (*len > 0U) {
    openplc_syslog_write(tag, buf, *len);
  }
  *len = 0U;
}

extern "C" void openplc_syslog_stdout(const char *ptr, size_t len)
{
  static char s_buf[BOOT_LOG_LINE_MAX];
  static size_t s_len;

  for (size_t i = 0U; i < len; i++) {
    line_put(s_buf, &s_len, "[sys] ", ptr[i]);
  }
}

size_t SystemLogPrint::write(uint8_t c)
{
  line_put(_line, &_len, _tag, (char)c);
  return 1U;
}

#if defined(USBCON) && defined(USBD_USE_CDC)
struct send_ctx {
  uint32_t next;   // first record not yet out; 0 = none sent
  bool stalled;
};

static void send_line(const char *line, size_t len, uint32_t seq, void *ctx)
{
  send_ctx *s = (send_ctx *)ctx;
  size_t n;

  if (s->stalled) {
    return;
  }
  n = SerialUSB.write((const uint8_t *)line, len);
  if (n == 0U) {
    s->stalled = true;
    return;
  }
  // A line cut short by a host that stopped reading still counts as sent:
  // resending it would repeat its start. The buffer keeps it for IAPTool.
  if ((n < len) || (SerialUSB.write((const uint8_t *)"\r\n", 2U) < 2U)) {
    s->stalled = true;
  }
  s->next = seq + 1U;
}

extern "C" USBD_HandleTypeDef hUSBD_Device_CDC;
extern "C" __IO bool dtrState;

/* R3-25: the host opening USB port 1 (DTR clear -> set) blinks the heartbeat
 * LED (LED3 on PE2, high = on, $PROD/docs/hardware/HARDWARE-FACTS.md) for
 * about 2 s, then hands PE2 back in the mode and level it had. Changes the
 * sketch makes to PE2 meanwhile become what is handed back. */
#define BLINK_MS       2000U
#define BLINK_HALF_MS  50U
#define LED_PIN        2U
#define LED_BIT        (1UL << LED_PIN)
#define LED_MODE_MASK  (3UL << (2U * LED_PIN))
#define LED_MODE_OUT   (1UL << (2U * LED_PIN))

static void led_take(void)
{
  uint32_t pm = __get_PRIMASK();

  // Read-modify-write of registers other pins share.
  __disable_irq();
  GPIOE->OTYPER &= ~LED_BIT;
  GPIOE->MODER = (GPIOE->MODER & ~LED_MODE_MASK) | LED_MODE_OUT;
  __set_PRIMASK(pm);
}

static void led_set(uint32_t level)
{
  GPIOE->BSRR = (level != 0U) ? LED_BIT : (LED_BIT << 16);
}

static void port_open_blink(void)
{
  static bool s_was_open;
  static bool s_blinking;
  static uint32_t s_start;
  static uint32_t s_toggled;
  static uint32_t s_mode;   // PE2 as it was: MODER bits, OTYPER bit, ODR bit
  static uint32_t s_otype;
  static uint32_t s_level;
  static uint32_t s_wrote;  // ODR bit last written here
  bool open = (hUSBD_Device_CDC.dev_state == USBD_STATE_CONFIGURED) && dtrState;
  uint32_t now = HAL_GetTick();

  if (open && !s_was_open) {
    if (!s_blinking) {
      __HAL_RCC_GPIOE_CLK_ENABLE();
      s_mode = GPIOE->MODER & LED_MODE_MASK;
      s_otype = GPIOE->OTYPER & LED_BIT;
      s_level = GPIOE->ODR & LED_BIT;
      s_blinking = true;
    }
    s_start = now;
    s_toggled = now;
    s_wrote = LED_BIT;
    led_set(s_wrote);
    led_take();
  }
  s_was_open = open;
  if (!s_blinking) {
    return;
  }

  if (((GPIOE->MODER & LED_MODE_MASK) != LED_MODE_OUT) || ((GPIOE->OTYPER & LED_BIT) != 0U)) {
    s_mode = GPIOE->MODER & LED_MODE_MASK;   // the sketch reconfigured PE2
    s_otype = GPIOE->OTYPER & LED_BIT;
    s_level = GPIOE->ODR & LED_BIT;
    led_set(s_wrote);
    led_take();
  } else if ((GPIOE->ODR & LED_BIT) != s_wrote) {
    s_level = GPIOE->ODR & LED_BIT;          // the sketch wrote PE2
    led_set(s_wrote);
  }

  if ((now - s_start) >= BLINK_MS) {
    uint32_t pm = __get_PRIMASK();

    led_set(s_level);
    __disable_irq();
    GPIOE->MODER = (GPIOE->MODER & ~LED_MODE_MASK) | s_mode;
    GPIOE->OTYPER = (GPIOE->OTYPER & ~LED_BIT) | s_otype;
    __set_PRIMASK(pm);
    s_blinking = false;
  } else if ((now - s_toggled) >= BLINK_HALF_MS) {
    s_toggled = now;
    s_wrote ^= LED_BIT;
    led_set(s_wrote);
  }
}
#endif

extern "C" void openplc_syslog_service(void)
{
#if defined(USBCON) && defined(USBD_USE_CDC)
  static bool s_busy;
  send_ctx s = {0U, false};

  if (s_busy) {
    return;
  }
  s_busy = true;
  port_open_blink();
  // CDC_connected(): configured, DTR set, and the host is still reading.
  if (log_on() && CDC_connected()) {
    (void)boot_log_read_unsent(send_line, &s);
    boot_log_mark_sent(s.next);
  }
  s_busy = false;
#endif
}

/* ---- Fault handlers: one [sys][FAULT] record, then reset (R3-22). ------- */

static size_t put_str(char *out, size_t n, const char *s)
{
  while (*s != '\0') {
    out[n++] = *s++;
  }
  return n;
}

static size_t put_hex(char *out, size_t n, const char *name, uint32_t v)
{
  n = put_str(out, n, name);
  n = put_str(out, n, "=0x");
  for (int i = 7; i >= 0; i--) {
    out[n + (size_t)i] = "0123456789abcdef"[v & 0xFU];
    v >>= 4;
  }
  return n + 8U;
}

/* frame: the exception stack frame (r0 r1 r2 r3 r12 lr pc xpsr). No printf:
 * the fault may have hit inside it. */
extern "C" __attribute__((used, noreturn)) void openplc_fault_report(const uint32_t *frame, uint32_t kind)
{
  static const char *const names[] = {"HardFault", "MemManage", "BusFault", "UsageFault"};
  char line[128];
  size_t n = 0U;

  n = put_str(line, n, BOOT_LOG_TAG_FAULT);
  n = put_str(line, n, names[kind & 3U]);
  n = put_hex(line, n, " pc", frame[6]);
  n = put_hex(line, n, " lr", frame[5]);
  n = put_hex(line, n, " cfsr", SCB->CFSR);
  n = put_hex(line, n, " hfsr", SCB->HFSR);
  n = put_hex(line, n, " bfar", SCB->BFAR);
  (void)boot_log_write(line, n);
  NVIC_SystemReset();
}

/* Picks the stack the frame was pushed on and hands it over. Basic asm only:
 * a naked function may hold nothing else. Weak so a sketch can define its
 * own handler (design must not restrict the app, $PROD M3/CONSTRAINTS.md). */
#define OPENPLC_FAULT_ENTRY(handler, kind)                                    \
  extern "C" __attribute__((naked, weak)) void handler(void)                  \
  {                                                                           \
    __asm volatile(                                                           \
      "tst lr, #4\n"                                                          \
      "ite eq\n"                                                              \
      "mrseq r0, msp\n"                                                       \
      "mrsne r0, psp\n"                                                       \
      "movs r1, #" #kind "\n"                                                 \
      "b openplc_fault_report\n");                                            \
  }

OPENPLC_FAULT_ENTRY(HardFault_Handler, 0)
OPENPLC_FAULT_ENTRY(MemManage_Handler, 1)
OPENPLC_FAULT_ENTRY(BusFault_Handler, 2)
OPENPLC_FAULT_ENTRY(UsageFault_Handler, 3)
