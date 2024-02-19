#ifndef LCD_H
#define LCD_H

#include <stdint.h>
#include "gd32vf103xb.h"
#include "b612-mono-24x40.h"

// PB5  -> SPI.DAT
// PB3  -> SPI.CLK
// PB6  -> RS/DC
// PB4  -> RESET
// PB8  -> CS

// #define LED_K_ON      GPIOB->ODR |=  (1 << 7);
// #define LED_K_OFF     GPIOB->ODR &= ~(1 << 7);

#define LED_RESET_ON  GPIOB->ODR |=  (1 << 4);
#define LED_RESET_OFF GPIOB->ODR &= ~(1 << 4);

#define LED_RS_ON     GPIOB->ODR |=  (1 << 6);
#define LED_RS_OFF    GPIOB->ODR &= ~(1 << 6);

#define LED_CS_ON     GPIOB->ODR |=  (1 << 8);
#define LED_CS_OFF    GPIOB->ODR &= ~(1 << 8);

#define SPI           SPI3


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


void LCD_write_bus (uint8_t dat) {
  while (SPI->SR & SPI_SR_BSY) {};
  LED_CS_OFF
  while (!(SPI->SR & SPI_SR_TXE)) {};
  *(uint8_t *)&(SPI->DR) = dat;
  while (SPI->SR & SPI_SR_BSY) {};
}

void LCD_WR_DATA8(uint8_t dat) {
  LED_RS_ON
  LCD_write_bus(dat);
}

void LCD_WR_DATA(uint16_t dat) {
  LED_RS_ON
  LCD_write_bus(dat >> 8);
  LCD_write_bus(dat & 0xff);
}


void LCD_WR_REG (uint8_t dat) {
  LED_RS_OFF
  LCD_write_bus(dat);
  LED_CS_ON
}

void LCD_Address_Set(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
    LCD_WR_REG(0x2a);
    LCD_WR_DATA(x1 + colstart);
    LCD_WR_DATA(x2 + colstart);
    LCD_WR_REG(0x2b);
    LCD_WR_DATA(y1 + rowstart);
    LCD_WR_DATA(y2 + rowstart);
    LCD_WR_REG(0x2c);
}

void setRotation(uint8_t m)
{
    rotation = m % 4;
    LCD_WR_REG(0x36);
    switch (rotation) {
    case 0:
        colstart = 0; //52;
        rowstart = 0; // 40;
        _width  = _init_width;
        _height = _init_height;
        LCD_WR_DATA8(TFT_MAD_COLOR_ORDER);
        break;

    case 1:
        colstart = 40;
        rowstart = 53;
        _width  = _init_height;
        _height = _init_width;
        LCD_WR_DATA8(TFT_MAD_MX | TFT_MAD_MV | TFT_MAD_COLOR_ORDER);
        break;
    case 2:
        colstart = 0; // 52;
        rowstart = 0; // 40;
        _width  = _init_width;
        _height = _init_height;
        LCD_WR_DATA8(TFT_MAD_MX | TFT_MAD_MY | TFT_MAD_COLOR_ORDER);
        break;
    case 3:
        colstart = 40;
        rowstart = 52;
        _width  = _init_height;
        _height = _init_width;
        LCD_WR_DATA8(TFT_MAD_MV | TFT_MAD_MY | TFT_MAD_COLOR_ORDER);
        break;
    }
}

void spi_config () {
  SPI->CR2 |= (
    SPI_CR2_TXDMAEN
  );

  SPI->CR1 &= ~SPI_CR1_BR_Msk;
  SPI->CR1 |= ( // MTU
    SPI_CR1_SSM  | // NSS software mode. The NSS level depends on SWNSS bit.
    SPI_CR1_SSI  | // NSS pin is pulled high
    SPI_CR1_MSTR | // Master mode
    (0x2 << SPI_CR1_BR_Pos) | // PCLK/8
    SPI_CR1_SPE    // SPI peripheral is enabled
  );
}

void lcd_init () {

  RCC->APB2ENR |= ( // clock enable
    RCC_APB2ENR_IOPAEN | // GPIOA
    RCC_APB2ENR_IOPBEN | // GPIOB
    RCC_APB2ENR_AFIOEN
  );

  // release: PB4, PA14
  AFIO->MAPR |=
      (0x4 << AFIO_MAPR_SWJ_CFG_Pos)
    // | (0x1 << AFIO_MARP_SPI1_REMAP_Pos)
    ;


  // Enable SPI3
  RCC->APB1ENR |= (RCC_APB1ENR_SPI3EN);
  // Reset the SPI3 peripheral.
  RCC->APB1RSTR &= ~(RCC_APB1RSTR_SPI3RST);

  // // Enable SPI1
  // RCC->APB2ENR |= (RCC_APB2ENR_SPI1EN);
  // // Reset the SPI1 peripheral.
  // RCC->APB2RSTR &= ~(RCC_APB2RSTR_SPI1RST);

  // RCC->AHBENR |= RCC_AHBENR_DMA1EN;

  // PB8  -> CS
  GPIOB->CRH = (GPIOB->CRH & ~(
    (GPIO_CRH_MODE8 | GPIO_CRH_CNF8) // CS
  )) | (
    (0x2 << GPIO_CRH_MODE8_Pos) // CS
  );

  // PB3  -> spi.sck -- high-speed alternate-function
  // PB4  -> RESET
  // PB5  -> spi.mosi -- high-speed alternate-function
  // PB6  -> RS/DC
  // PB7  <- IR
  GPIOB->CRL = (GPIOB->CRL & ~(
    (GPIO_CRL_MODE3  | GPIO_CRL_CNF3) | // spi.sck
    (GPIO_CRL_MODE4  | GPIO_CRL_CNF4) | // reset
    (GPIO_CRL_MODE5  | GPIO_CRL_CNF5) | // spi.mosi
    (GPIO_CRL_MODE6  | GPIO_CRL_CNF6) | // rs
    (GPIO_CRL_MODE7  | GPIO_CRL_CNF7) // IR
  )) | (
    (0x3 << GPIO_CRL_MODE3_Pos) | (0x2 << GPIO_CRL_CNF3_Pos) | // spi.sck
    (0x2 << GPIO_CRL_MODE4_Pos) | // reset
    (0x3 << GPIO_CRL_MODE5_Pos) | (0x2 << GPIO_CRL_CNF5_Pos) | // spi.mosi
    (0x2 << GPIO_CRL_MODE6_Pos) | // rs
    (0x0 << GPIO_CRL_MODE7_Pos) | (0x2 << GPIO_CRL_CNF7_Pos) // IR pull-???
  );

  GPIOB->ODR |= (1 << 7); // IR pull-up

  // // PA14 -> LEDK
  // GPIOA->CRH = GPIOA->CRH & ~(
  //   (GPIO_CRH_MODE14 | GPIO_CRH_CNF14)
  // ) | (
  //   (0x2 << GPIO_CRH_MODE14_Pos) | (0x0 << GPIO_CRH_CNF14_Pos)
  // );

  spi_config();

  LED_RESET_OFF
  LED_RS_OFF
  delay_us(10);
  LED_RESET_ON
  delay_us(10);
  LCD_WR_REG(ST7789_SLPOUT);   // Sleep out
  delay_us(10);
  LCD_WR_REG(ST7789_NORON);    // Normal display mode on
  delay_us(10);

  //------------------------------display and color format setting--------------------------------//
  LCD_WR_REG(ST7789_MADCTL);
  //LCD_WR_DATA8(0x00);
  LCD_WR_DATA8(TFT_MAD_RGB);

  // JLX240 display datasheet
  LCD_WR_REG(0xB6);
  LCD_WR_DATA8(0x0A);
  LCD_WR_DATA8(0x82);

  LCD_WR_REG(ST7789_COLMOD);
  LCD_WR_DATA8(0x55); // 16bit/pixel 5-6-5

  delay_us(10);

  //--------------------------------ST7789V Frame rate setting----------------------------------//
  LCD_WR_REG(ST7789_PORCTRL);
  LCD_WR_DATA8(0x0c);
  LCD_WR_DATA8(0x0c);
  LCD_WR_DATA8(0x00);
  LCD_WR_DATA8(0x33);
  LCD_WR_DATA8(0x33);

  LCD_WR_REG(ST7789_GCTRL);      // Voltages: VGH / VGL
  LCD_WR_DATA8(0x35);

  //---------------------------------ST7789V Power setting--------------------------------------//
  LCD_WR_REG(ST7789_VCOMS);
  LCD_WR_DATA8(0x28);       // JLX240 display datasheet

  LCD_WR_REG(ST7789_LCMCTRL);
  LCD_WR_DATA8(0x0C);

  LCD_WR_REG(ST7789_VDVVRHEN);
  LCD_WR_DATA8(0x01);
  LCD_WR_DATA8(0xFF);

  LCD_WR_REG(ST7789_VRHS);       // voltage VRHS
  LCD_WR_DATA8(0x10); // 4.35+( vcom+vcom offset+vdv)

  LCD_WR_REG(ST7789_VDVSET);
  LCD_WR_DATA8(0x1c); // 0x20);

  LCD_WR_REG(ST7789_FRCTR2);
  LCD_WR_DATA8(0x0f);

  LCD_WR_REG(ST7789_PWCTRL1);
  LCD_WR_DATA8(0xa4);
  LCD_WR_DATA8(0xa1);

  LCD_WR_REG(ST7789_WRCTRLD);
  LCD_WR_DATA8(0x24);


  //--------------------------------ST7789V gamma setting---------------------------------------//
  LCD_WR_REG(ST7789_PVGAMCTRL);
  LCD_WR_DATA8(0xd0);
  LCD_WR_DATA8(0x00);
  LCD_WR_DATA8(0x02);
  LCD_WR_DATA8(0x07);
  LCD_WR_DATA8(0x0a);
  LCD_WR_DATA8(0x28);
  LCD_WR_DATA8(0x32);
  LCD_WR_DATA8(0x44);
  LCD_WR_DATA8(0x42);
  LCD_WR_DATA8(0x06);
  LCD_WR_DATA8(0x0e);
  LCD_WR_DATA8(0x12);
  LCD_WR_DATA8(0x14);
  LCD_WR_DATA8(0x17);

  LCD_WR_REG(ST7789_NVGAMCTRL);
  LCD_WR_DATA8(0xd0);
  LCD_WR_DATA8(0x00);
  LCD_WR_DATA8(0x02);
  LCD_WR_DATA8(0x07);
  LCD_WR_DATA8(0x0a);
  LCD_WR_DATA8(0x28);
  LCD_WR_DATA8(0x31);
  LCD_WR_DATA8(0x54);
  LCD_WR_DATA8(0x47);
  LCD_WR_DATA8(0x0e);
  LCD_WR_DATA8(0x1c);
  LCD_WR_DATA8(0x17);
  LCD_WR_DATA8(0x1b);
  LCD_WR_DATA8(0x1e);

  LCD_WR_REG(ST7789_INVON);

  LCD_WR_REG(ST7789_CASET);    // Column address set
  LCD_WR_DATA8(0x00);
  LCD_WR_DATA8(0x00);
  LCD_WR_DATA8(0x00);
  LCD_WR_DATA8(0xE5);    // 239

  LCD_WR_REG(ST7789_RASET);    // Row address set
  LCD_WR_DATA8(0x00);
  LCD_WR_DATA8(0x00);
  LCD_WR_DATA8(0x01);
  LCD_WR_DATA8(0x3F);    // 319


  delay_us(10);

  setRotation(0);

  LCD_WR_REG(ST7789_DISPON);    //Display on
  delay_us(10);
}

int colors [] = {
  // 0, // black
  // 0xaaaa, // grey
  // 0xffff, // white
  // 0x0ee0, // green
  // 0xeee0  // yellow
  //  R     G    B
  //.....******.....
  0b0000000000000000, //  0 Black
  0b1111100000000000, //  1 Red
  0b0000011111100000, //  2 Green
  0b1111111111100000, //  3 Yellow
  0b0000000000011111, //  4 Blue
  0b1111100000011111, //  5 Magenta
  0b0000011111111111, //  6 Cyan
  0b0111101111101111, //  7 White
  //.....******.....
  0b0011100011100111, //  8 Bright Black
  0b1111100011100111, //  9 Bright Red
  0b0011111111100111, // 10 Bright Green
  0b1111111111100111, // 11 Bright Yellow
  0b0011100011111111, // 12 Bright Blue
  0b1111100011111111, // 13 Bright Magenta
  0b0011111111111111, // 14 Bright Cyan
  0b1111111111111111, // 15 Bright White
  //.....******.....
};
void lcd_fill (uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color) {
  uint16_t i, j;
  LCD_Address_Set(x0, y0, x1, y1);
  for (i = x0; i <= x1; i++) {
    for (j = y0; j <= y1; j++) {
      LCD_WR_DATA(color);
    }
  }
}

void LCD_ShowChar(uint16_t x, uint16_t y, int num, uint8_t mode, uint16_t color) {
  LCD_Address_Set(x, y, x + 24 - 1, y + 40 - 1);
  for (int pos = 0; pos < 40; pos++) {
    for (int xx = 0; xx < 3; xx++) {
      int temp = b612_24_40[num * 40 * 3 + pos * 3 + xx];
      for (int t = 0; t < 8; t++) {
        if (temp & 1)LCD_WR_DATA(color);
        else LCD_WR_DATA(BACK_COLOR);
        temp >>= 1;
      }
    }
  }
}


#endif
