#ifndef LCD_H
#define LCD_H

#include <stdint.h>
#include "driver/spi_master.h"

#define ST7789_SLPOUT       0x11
#define ST7789_NORON        0x13
#define ST7789_MADCTL       0x36      // Memory data access control
#define TFT_MAD_RGB         0x08
#define ST7789_COLMOD       0x3A
#define ST7789_PORCTRL      0xB2      // Porch control
#define ST7789_GCTRL        0xB7      // Gate control
#define ST7789_VCOMS        0xBB      // VCOMS setting
#define ST7789_LCMCTRL      0xC0      // LCM control
#define ST7789_VDVVRHEN     0xC2      // VDV and VRH command enable
#define ST7789_VRHS         0xC3      // VRH set
#define ST7789_VDVSET       0xC4      // VDV setting
#define ST7789_FRCTR2       0xC6      // FR Control 2
#define ST7789_PWCTRL1      0xD0      // Power control 1
#define ST7789_PVGAMCTRL    0xE0      // Positive voltage gamma control
#define ST7789_NVGAMCTRL    0xE1      // Negative voltage gamma control
#define ST7789_INVON        0x21
#define ST7789_CASET        0x2A
#define ST7789_RASET        0x2B
#define ST7789_RAMWR        0x2C
#define ST7789_DISPOFF      0x28
#define ST7789_DISPON       0x29
#define ST7789_WRCTRLD      0x53      // Write CTRL Display

#define TFT_MAD_COLOR_ORDER TFT_MAD_RGB
#define TFT_MAD_MY          0x80
#define TFT_MAD_MX          0x40
#define TFT_MAD_MV          0x20
#define TFT_MAD_ML          0x10

uint16_t BACK_COLOR;

uint16_t _init_height = 240;
uint16_t _init_width = 320; // 135;

uint16_t _width = 320; // 135;
uint16_t _height = 240;

uint16_t colstart = 52;
uint16_t rowstart = 40;

uint8_t rotation = 0;



static spi_device_handle_t spi;

// https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-reference/peripherals/spi_master.html

void lcd_spi_pre_transfer_callback(spi_transaction_t *t) {
  int dc = (int)t->user;
  gpio_set_level(DISPLAY_RS, dc);
}

// SPI LCD

spi_bus_config_t buscfg = {
  .miso_io_num = -1,
  .mosi_io_num = DISPLAY_DATA,
  .sclk_io_num = DISPLAY_CLOCK,
  .quadwp_io_num = -1,
  .quadhd_io_num = -1,
  .max_transfer_sz = 16 * 320 * 2 + 8
};

spi_device_interface_config_t devcfg = {
  .clock_speed_hz = 10 * 1000 * 1000,     //Clock out at 10 MHz
  .mode = 0,                              //SPI mode 0
  .spics_io_num = DISPLAY_CS,             //CS pin
  .queue_size = 7,                        //We want to be able to queue 7 transactions at a time
  .pre_cb = lcd_spi_pre_transfer_callback, //Specify pre-transfer callback to handle D/C line
};

void lcd_cmd(spi_device_handle_t spi, const uint8_t cmd, bool keep_cs_active) {
  esp_err_t ret;
  spi_transaction_t t;
  memset(&t, 0, sizeof(t));       // Zero out the transaction
  t.length = 8;                   // Command is 8 bits
  t.tx_buffer = &cmd;             // The data is the cmd itself
  t.user = (void*)0;              // D/C needs to be set to 0
  if (keep_cs_active) {
    t.flags = SPI_TRANS_CS_KEEP_ACTIVE;   //Keep CS active after data transfer
  }
  ret = spi_device_polling_transmit(spi, &t);  // Transmit!
  assert(ret == ESP_OK);            //Should have had no issues.
}

void lcd_data(spi_device_handle_t spi, const uint8_t *data, int len) {
  if (len == 0) return;           // no need to send anything
  esp_err_t ret;
  spi_transaction_t t;
  memset(&t, 0, sizeof(t));       // Zero out the transaction
  t.length = len * 8;             // Len is in bytes, transaction length is in bits.
  t.tx_buffer = data;             // Data
  t.user = (void*)1;              // D/C needs to be set to 1
  ret = spi_device_polling_transmit(spi, &t); // Transmit!
  assert(ret == ESP_OK);          //Should have had no issues.
}

void play_seq(spi_device_handle_t spi, const uint8_t *seq) {
  size_t idx = 0;
  while (1) {
    const uint8_t cmd = seq[idx];
    idx += 1;
    if (cmd == 0) {
      break;
    }
    if (cmd == 0xff) {
      lcd_cmd(spi, seq[idx], false);
      idx += 1;
      continue;
    }
    if (cmd == 0xfe) {
      vTaskDelay(10 / portTICK_PERIOD_MS); // 10ms polling
      continue;
    }
    lcd_data(spi, &seq[idx], cmd);
    idx += cmd;
  }
}

static uint8_t init_seq[] = {
  255, ST7789_SLPOUT, // Sleep out
  0xfe, // delay
  255, ST7789_NORON, // Normal display mode on
  0xfe, // delay
  //------------------------------display and color format setting--------------------------------//
  255, ST7789_MADCTL,
  1, TFT_MAD_RGB,
  // JLX240 display datasheet
  255, 0xB6,
  2, 0x0A, 0x82,
  255, ST7789_COLMOD,
  1, 0x55, // 16bit/pixel 5-6-5
  0xfe, // delay
  //--------------------------------ST7789V Frame rate setting----------------------------------//
  255, ST7789_PORCTRL,
  5, 0x0c, 0x0c, 0x00, 0x33, 0x33,
  255, ST7789_GCTRL, // Voltages: VGH / VGL
  1, 0x35,
  //---------------------------------ST7789V Power setting--------------------------------------//
  255, ST7789_VCOMS,
  1, 0x28, // JLX240 display datasheet
  255, ST7789_LCMCTRL,
  1, 0x0C,
  255, ST7789_VDVVRHEN,
  2, 0x01, 0xFF,







  // lcd_cmd(spi, ST7789_DISPON, false); // Display on
  // vTaskDelay(10 / portTICK_PERIOD_MS);
  // 0xff, 0x44, // Set RAM X - address Start / End position
  // 2, 0, (EWIDTH - 1) >> 3,

  // 0xff, 0x45, // Set Ram Y - address Start / End position
  // 4, 0, 0, (EHEIGHT - 1) & 0xff, ((EHEIGHT - 1) >> 8) & 0xff,

  // 0xff, 0x4e, // Set RAM X address counter
  // 1, 0,

  // 0xff, 0x4f, // Set RAM Y address counter
  // 2, 0, 0,

  // 0xff, 0x21, // Display Update Control
  // 2,
  // (4 << 0) | // Normal: 0, BypassAs0: 4, Inverse: 8
  // (4 << 4),  // Normal: 0, BypassAs0: 4, Inverse: 8
  // 0,         // Source Output Mode

  // 0xff, 0x22, // Full Update
  // 1, 0xf4,

  // 0xff, 0x20,// Master Activation
  // 0xfe, // check busy
  0xff, ST7789_DISPON, // Display on
  0xfe, // delay
  0 // THE END
};

static uint8_t addr_seq[] = {
  255, 0x2a,
  2, 0, 0,
  255, 0x2b,
  2, 0, 0,
  255, 0x2c,
  0 // THE END
};

static uint16_t pixel_buffer[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

void LCD_Address_Set(spi_device_handle_t spi, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
  addr_seq[3] = x1 + colstart;
  addr_seq[4] = x2 + colstart;
  addr_seq[8] = y1 + rowstart;
  addr_seq[9] = y2 + rowstart;
  play_seq(spi, init_seq);
}

void lcd_fill (spi_device_handle_t spi, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color) {
  uint16_t i, j;
  LCD_Address_Set(spi, x0, y0, x1, y1);
  for (i = x0; i <= x1; i++) {
    for (j = y0; j <= y1; j++) {
      pixel_buffer[0] = color; // Blue
      lcd_data(spi, pixel_buffer, 4);
    }
  }
}

void lcd_init(spi_device_handle_t spi) {
  // init spi
  ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));
  ESP_ERROR_CHECK(spi_bus_add_device(LCD_HOST, &devcfg, &spi));
  //Initialize non-SPI GPIOs
  gpio_config_t io_conf = {
    .intr_type = GPIO_INTR_DISABLE,
    .mode = GPIO_MODE_OUTPUT,
    .pin_bit_mask = ((1ULL << DISPLAY_RS) | (1ULL << DISPLAY_RESET)),
    .pull_up_en = true,
    .pull_down_en = 0
  };
  gpio_config(&io_conf);

  //Reset the display
  gpio_set_level(DISPLAY_RESET, 0);
  gpio_set_level(DISPLAY_RS, 0);
  vTaskDelay(10 / portTICK_PERIOD_MS);
  gpio_set_level(DISPLAY_RESET, 1);
  vTaskDelay(10 / portTICK_PERIOD_MS);

  play_seq(spi, init_seq);
  lcd_fill(spi, 0, 0, 16, 16, 0x000);
}

#endif
