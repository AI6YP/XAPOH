#include "link.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_rom_gpio.h"
#include "soc/gpio_sig_map.h" // U1TXD_OUT_IDX, SIG_GPIO_OUT_IDX

#define LINK_UART UART_NUM_1
#define LINK_BAUD 4800

static const int link_gpio[3] = {0, 1, 2};

static int cur = -1;                    // gpio currently bound to U1TXD (-1 = none)
static SemaphoreHandle_t link_lock;     // serialize send vs flash

// Park a pin as a plain GPIO driven high (UART/SWIO idle level).
static void park_gpio(int g) {
  esp_rom_gpio_connect_out_signal(g, SIG_GPIO_OUT_IDX, false, false);
  gpio_set_level(g, 1);
}

// Point U1TXD at `g`. The line idles high, so re-pointing never glitches low.
static void bind_txd(int g) {
  if (cur == g) return;
  if (cur >= 0) park_gpio(cur);         // release previous owner, hold it idle-high
  esp_rom_gpio_connect_out_signal(g, U1TXD_OUT_IDX, false, false);
  cur = g;
}

void link_init(void) {
  link_lock = xSemaphoreCreateMutex();

  const uart_config_t uc = {
    .baud_rate  = LINK_BAUD,
    .data_bits  = UART_DATA_8_BITS,
    .parity     = UART_PARITY_DISABLE,
    .stop_bits  = UART_STOP_BITS_1,
    .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
    .source_clk = UART_SCLK_DEFAULT,
  };
  ESP_ERROR_CHECK(uart_driver_install(LINK_UART, 256, 256, 0, NULL, 0));
  ESP_ERROR_CHECK(uart_param_config(LINK_UART, &uc));
  // Deliberately NOT uart_set_pin: TXD is bound to a chosen gpio per send.

  for (int i = 0; i < 3; i++) {
    gpio_reset_pin(link_gpio[i]);
    gpio_set_direction(link_gpio[i], GPIO_MODE_OUTPUT);
    park_gpio(link_gpio[i]);            // idle high
  }
}

void link_send(int idx, const uint8_t *buf, size_t len) {
  if (idx < 0 || idx > 2) return;
  xSemaphoreTake(link_lock, portMAX_DELAY);
  bind_txd(link_gpio[idx]);
  uart_write_bytes(LINK_UART, buf, len);
  uart_wait_tx_done(LINK_UART, pdMS_TO_TICKS(50));
  xSemaphoreGive(link_lock);
}

int link_gpio_num(int idx) {
  return (idx >= 0 && idx <= 2) ? link_gpio[idx] : -1;
}

void link_flash_begin(int idx) {
  xSemaphoreTake(link_lock, portMAX_DELAY); // held across the whole flash
  int g = link_gpio_num(idx);
  if (cur == g && g >= 0) {                 // hand the raw pin to SWIO
    esp_rom_gpio_connect_out_signal(g, SIG_GPIO_OUT_IDX, false, false);
    cur = -1;
  }
}

void link_flash_end(int idx) {
  int g = link_gpio_num(idx);
  if (g >= 0) {
    gpio_set_direction(g, GPIO_MODE_OUTPUT);
    park_gpio(g);                           // idle-high; next send rebinds U1TXD
  }
  xSemaphoreGive(link_lock);
}
