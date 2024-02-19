#ifndef MAPC_H
#define MAPC_H

#include <stdint.h>
#include "gd32vf103xb.h"

// PB10 -> I2C1_SCL of 0,1
// PB11 -> I2C1_SDA of 0,1

#define I2C I2C2

volatile uint8_t temp;

void mapc_init () {

  RCC->APB2ENR |= ( // enable ports, Alternative Functions
    RCC_APB2ENR_IOPBEN | // GPIOB
    RCC_APB2ENR_AFIOEN
  );

  // Enable clock for I2C2 of [1, 2]
  RCC->APB1ENR |= (RCC_APB1ENR_I2C2EN);






  // // Bit-Bang START-STOP

  // GPIOB->CRH = (GPIOB->CRH & ~(
  //   (GPIO_CRH_MODE10 | GPIO_CRH_CNF10) | // PB10 -> SCL
  //   (GPIO_CRH_MODE11 | GPIO_CRH_CNF11) | // PB11 -> SDA
  //   0
  // )) | (
  //   (0x2 << GPIO_CRH_MODE10_Pos) | // PB10 -> SCL
  //   (0x2 << GPIO_CRH_MODE11_Pos) | // PB11 -> SDA
  //   0
  // );

  // for (int i = 0; i < 100; i++) {
  //   GPIOB->ODR |=  (1 << 10); // SCL=1
  //   GPIOB->ODR |=  (1 << 11); // SDA=1

  //   delay_ms(100);

  //   GPIOB->ODR &= ~(1 << 10); // SCL=0
  //   GPIOB->ODR &= ~(1 << 11); // SDA=0

  //   delay_ms(100);
  // }

  // return;






  GPIOB->CRH = (GPIOB->CRH & ~(
    (GPIO_CRH_MODE10 | GPIO_CRH_CNF10) | // PB10 -> SCL
    (GPIO_CRH_MODE11 | GPIO_CRH_CNF11) | // PB11 -> SDA
    0
  )) | (
    //     MD: Z, 10, 2, 50 MHz Output           CTL: AFIO + Open-Drain
    (0x2 << GPIO_CRH_MODE11_Pos) | (0x3 << GPIO_CRH_CNF11_Pos) | // PB11 -> SDA
    (0x2 << GPIO_CRH_MODE10_Pos) | (0x3 << GPIO_CRH_CNF10_Pos) | // PB10 -> SCL
    0
  );



  // GPIOB->ODR |= (1 << 10) | (1 << 11); // pull-up

  I2C->CTL0 |=  (1 << 15); // [15] SRESET : SW reset
  I2C->CTL0 &= ~(1 << 15); // [15] SREAST : Normal operation

  // Set clock
  I2C->CTL1 |= (
    (63 << 0) | // I2/sCCLK[5:0] ???
    // (55 << 0) | // I2/sCCLK[5:0] ???
    // (16 << 0) | // I2CCLK[5:0] ???
    0
  );
  I2C->CKCFG |= (
    // (31 << 0) | // CLKC[11:0] ???
    (55 << 0) | // CLKC[11:0] ???
    // (255 << 0) | // CLKC[11:0] ???
    (1 << 15) | // FAST
    // (1 << 14) | // DTCY
    // (55 << 0) | // CLKC[11:0] ???
    0
  );
  // Rise Time
  I2C->RT = 7;  // ???

  I2C->CTL0 |=  (1 << 0);  // [0]  I2CEN  : enable I2C



  // I2C->CTL0 |=  (1 << 8);  // [8]  START  : send I2C START
  // while (!(I2C->STAT0 & (1 << 0)));  // wait for [0] SBSEND bit to set

  // // I2C->CTL0 &= ~(1 << 10);  // clear the ACK bit
  // // temp = I2C->STAT0 | I2C->STAT1; // read SR1 and SR2 to clear the ADDR bit.... EV6 condition
  // I2C->CTL0 |=  (1 << 9);  // STOP
  // while (!(I2C->STAT0 & (1 << 0)));  // wait for [0] SBSEND bit to set

  // delay_us(1000);



  // I2C->CTL0 |=  (1 << 10); // [10] ACEN   : enable the ACK
  // I2C->CTL0 |=  (1 << 8);  // [8]  START  : send I2C START
  // while (!(I2C->STAT0 & (1 << 0)));  // wait for [0] SBSEND bit to set

  // // READ 1 byte
  // //            0101     : AD7417
  // //                000  : A2,A1,A0 pins
  // //                   0 : Write
  // I2C->DATA = 0b01010000; //  send the address
  // while (!(I2C->STAT0 & (1 << 1)));  // wait for [1] ADDSEND bit to set
  // while (I2C->STAT1 & (1 << 11));

  // while (!(I2C->STAT0 & (1 << 7))); // check [7] TBE bit
  // I2C->DATA = 0; //  send data
  // while (!(I2C->STAT0 & (1 << 2))); // check [2] BTC bit Byte transmission completed.

  // // I2C->CTL0 &= ~(1 << 10);  // clear the ACK bit
  // // temp = I2C->STAT0 | I2C->STAT1; // read SR1 and SR2 to clear the ADDR bit.... EV6 condition
  // // I2C->CTL0 |=  (1 << 9);  // STOP

  // // while (!(I2C1->STAT0 & (1 << 6)));  // wait for RxNE to set

  // // // buffer[size-remaining] = I2C1->DR;  // Read the data from the DATA REGISTER

  // I2C->CTL0 &= ~(1 << 10);  // clear the ACK bit
  // temp = I2C->STAT0 | I2C->STAT1; // read SR1 and SR2 to clear the ADDR bit.... EV6 condition
  // I2C->CTL0 |=  (1 << 9);  // STOP

}

int mapc_read () {
  return 0;
}

#endif
