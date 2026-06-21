#include "ch32fun.h"
#include <string.h>
#include "i2c_slave.h"

volatile uint8_t i2c_registers[32] = {0x00};

void onWrite(uint8_t reg, uint8_t length) {
  uint8_t reg0 = i2c_registers[0];
  funDigitalWrite(PD4, (reg0 >> 0) & 1);
  funDigitalWrite(PD5, (reg0 >> 1) & 1);
  funDigitalWrite(PD6, (reg0 >> 2) & 1);
  funDigitalWrite(PD7, (reg0 >> 3) & 1);
  funDigitalWrite(PA1, (reg0 >> 4) & 1);
  funDigitalWrite(PA2, (reg0 >> 5) & 1);
  funDigitalWrite(PC0, (reg0 >> 6) & 1);

  uint8_t reg1 = i2c_registers[1];
  funDigitalWrite(PC3, (reg1 >> 0) & 1);
  funDigitalWrite(PC4, (reg1 >> 1) & 1);
  funDigitalWrite(PC5, (reg1 >> 2) & 1);
  funDigitalWrite(PC6, (reg1 >> 3) & 1);
  funDigitalWrite(PC7, (reg1 >> 4) & 1);
  funDigitalWrite(PD2, (reg1 >> 5) & 1);
  funDigitalWrite(PD3, (reg1 >> 6) & 1);
}

int main () {
  SystemInit();

  RCC->CFGR0 = RCC_HPRE_DIV16; // PLLCLK = HCLK = SYSCLK = APB1
  RCC->CTLR  = RCC_HSION;
  // (((FUNCONF_HSITRIM) << 3) | RCC_HSION | HSEBYP | RCC_CSS) | RCC_HSION; // Use HSI, Only.


  funGpioInitAll();

  funPinMode(PD4, GPIO_Speed_50MHz | GPIO_CNF_OUT_PP); // 1 LED
  funPinMode(PD5, GPIO_Speed_50MHz | GPIO_CNF_OUT_PP); // 2 LED
  funPinMode(PD6, GPIO_Speed_50MHz | GPIO_CNF_OUT_PP); // 3
  funPinMode(PD7, GPIO_Speed_50MHz | GPIO_CNF_OUT_PP); // 4
  funPinMode(PA1, GPIO_Speed_50MHz | GPIO_CNF_OUT_PP); // 5
  funPinMode(PA2, GPIO_Speed_50MHz | GPIO_CNF_OUT_PP); // 6
  // 7 GND
  funPinMode(PD0, GPIO_Speed_In | GPIO_CNF_IN_PUPD); // 8
 	// GPIO_pinMode(GPIOv_from_PORT_PIN(GPIO_port_D, 0), GPIO_pinMode_I_pullUp, GPIO_Speed_In);
  // pGPIO->CFGLR |= (GPIO_CNF_IN_PUPD << (4*u8Pin));
  // GPIO_Speed_In	| GPIO_CNF_IN_PUPD
	// pGPIO->BSHR = (1 << u8Pin);
  // 9 VDD
  funPinMode(PC0, GPIO_Speed_50MHz | GPIO_CNF_OUT_PP); // 10

  funPinMode(PC1, GPIO_CFGLR_OUT_10Mhz_AF_OD); // 11  SDA
  funPinMode(PC2, GPIO_CFGLR_OUT_10Mhz_AF_OD); // 12  SCL
  funPinMode(PC3, GPIO_Speed_50MHz | GPIO_CNF_OUT_PP); // 13
  funPinMode(PC4, GPIO_Speed_50MHz | GPIO_CNF_OUT_PP); // 14
  funPinMode(PC5, GPIO_Speed_50MHz | GPIO_CNF_OUT_PP); // 15
  funPinMode(PC6, GPIO_Speed_50MHz | GPIO_CNF_OUT_PP); // 16
  funPinMode(PC7, GPIO_Speed_50MHz | GPIO_CNF_OUT_PP); // 17
  // 18 SWIO
  funPinMode(PD2, GPIO_Speed_50MHz | GPIO_CNF_OUT_PP); // 19
  funPinMode(PD3, GPIO_Speed_50MHz | GPIO_CNF_OUT_PP); // 20

  SetupI2CSlave(
    (0xa | (GPIOD->INDR & 1)), // PD0 = A0  7-bit address (0x08 ... 0x77)
    i2c_registers,
    sizeof(i2c_registers),
    onWrite,
    NULL,
    false
  );

  while (1) {
    __WFE();
    // Delay_Ms(250);
  }

  return 0;
}
