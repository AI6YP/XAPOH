#include <string.h>
#include "gd32vf103xb.h"
#include "lcd-xapoh.h"
#include "mapc.h"
// #include "st7789.h"

// Y1  -> PA5
// Y2  -> PA3
// Y3  -> PA8
// Y4  -> PA2
// Y5  -> PA1
// Y5N -> PA0
// Y1N -> PB13
// Y3N -> PB14
// Y7  -> PC13
// Y8  -> PC14
// A   -> PB12
// AN  -> PC15
// B   -> PA12
// BN  -> PA11
// J   -> PA10
// JN  -> PA13
// Q   -> PA8
// QN  -> PB15
// PTT -> PB9
// PTN -> PA9
// MA  -> PB0
// MB  -> PB1
// ON  -> PA4

#define Y1_1  GPIOA->ODR |=  (1 << 5);
#define Y1_0  GPIOA->ODR &= ~(1 << 5);

#define Y2_1  GPIOA->ODR |=  (1 << 3);
#define Y2_0  GPIOA->ODR &= ~(1 << 3);

#define Y3_1  GPIOA->ODR |=  (1 << 6);
#define Y3_0  GPIOA->ODR &= ~(1 << 6);

#define Y4_1  GPIOA->ODR |=  (1 << 2);
#define Y4_0  GPIOA->ODR &= ~(1 << 2);

#define Y5_1  GPIOA->ODR |=  (1 << 1);
#define Y5_0  GPIOA->ODR &= ~(1 << 1);

#define Y5N1  GPIOA->ODR |=  (1 << 0);
#define Y5N0  GPIOA->ODR &= ~(1 << 0);

#define Y1N1  GPIOB->ODR |=  (1 << 13);
#define Y1N0  GPIOB->ODR &= ~(1 << 13);

#define Y3N1  GPIOB->ODR |=  (1 << 14);
#define Y3N0  GPIOB->ODR &= ~(1 << 14);

#define Y7_1  GPIOC->ODR |=  (1 << 13);
#define Y7_0  GPIOC->ODR &= ~(1 << 13);

#define Y8_1  GPIOC->ODR |=  (1 << 14);
#define Y8_0  GPIOC->ODR &= ~(1 << 14);

#define A__1  GPIOB->ODR |=  (1 << 12);
#define A__0  GPIOB->ODR &= ~(1 << 12);

#define AN_1  GPIOC->ODR |=  (1 << 15);
#define AN_0  GPIOC->ODR &= ~(1 << 15);

#define B__1  GPIOA->ODR |=  (1 << 12);
#define B__0  GPIOA->ODR &= ~(1 << 12);

#define BN_1  GPIOA->ODR |=  (1 << 11);
#define BN_0  GPIOA->ODR &= ~(1 << 11);

#define J__1  GPIOA->ODR |=  (1 << 10);
#define J__0  GPIOA->ODR &= ~(1 << 10);

#define JN_1  GPIOA->ODR |=  (1 << 13);
#define JN_0  GPIOA->ODR &= ~(1 << 13);

#define Q__1  GPIOA->ODR |=  (1 << 8);
#define Q__0  GPIOA->ODR &= ~(1 << 8);

#define QN_1  GPIOB->ODR |=  (1 << 15);
#define QN_0  GPIOB->ODR &= ~(1 << 15);

#define PTT1  GPIOB->ODR |=  (1 << 9);
#define PTT0  GPIOB->ODR &= ~(1 << 9);

#define PTN1  GPIOA->ODR |=  (1 << 9);
#define PTN0  GPIOA->ODR &= ~(1 << 9);

#define MA_1  GPIOB->ODR |=  (1 << 0);
#define MA_0  GPIOB->ODR &= ~(1 << 0);

#define MB_1  GPIOB->ODR |=  (1 << 1);
#define MB_0  GPIOB->ODR &= ~(1 << 1);

#define ON_1  GPIOA->ODR |=  (1 << 4);
#define ON_0  GPIOA->ODR &= ~(1 << 4);




void port_init () {

  RCC->APB2ENR |= ( // clock enable
    RCC_APB2ENR_IOPAEN | // GPIOA
    RCC_APB2ENR_IOPBEN | // GPIOB
    RCC_APB2ENR_IOPCEN | // GPIOC
    RCC_APB2ENR_AFIOEN
  );


  GPIOA->CRL = (GPIOA->CRL & ~(
    (GPIO_CRL_MODE0  | GPIO_CRL_CNF0) | // Y5N -> PA0
    (GPIO_CRL_MODE1  | GPIO_CRL_CNF1) | // Y5  -> PA1
    (GPIO_CRL_MODE2  | GPIO_CRL_CNF2) | // Y4 -> PA2
    (GPIO_CRL_MODE3  | GPIO_CRL_CNF3) | // Y2 -> PA3
    (GPIO_CRL_MODE4  | GPIO_CRL_CNF4) | // ON  -> PA4
    (GPIO_CRL_MODE5  | GPIO_CRL_CNF5) | // Y1 -> PA5
    (GPIO_CRL_MODE6  | GPIO_CRL_CNF6) | // Y3 -> PA6
    0
  )) | (
    (0x2 << GPIO_CRL_MODE0_Pos) | // Y5N
    (0x2 << GPIO_CRL_MODE1_Pos) | // Y5
    (0x2 << GPIO_CRL_MODE2_Pos) | // Y4
    (0x2 << GPIO_CRL_MODE3_Pos) | // Y2
    (0x2 << GPIO_CRL_MODE4_Pos) | // ON
    (0x2 << GPIO_CRL_MODE5_Pos) | // Y1
    (0x2 << GPIO_CRL_MODE6_Pos) | // Y3
    0
  );

  GPIOA->CRH = (GPIOA->CRH & ~(
    (GPIO_CRH_MODE8  | GPIO_CRH_CNF8 ) | // Q   -> PA8
    (GPIO_CRH_MODE9  | GPIO_CRH_CNF9 ) | // PTN -> PA9
    (GPIO_CRH_MODE10 | GPIO_CRH_CNF10) | // J   -> PA10
    (GPIO_CRH_MODE11 | GPIO_CRH_CNF11) | // BN  -> PA11
    (GPIO_CRH_MODE12 | GPIO_CRH_CNF12) | // B   -> PA12
    (GPIO_CRH_MODE13 | GPIO_CRH_CNF13) | // JN  -> PA13
    0
  )) | (
    (0x2 << GPIO_CRH_MODE8_Pos ) | // Q   -> PA8
    (0x2 << GPIO_CRH_MODE9_Pos ) | // PTN -> PA9
    (0x2 << GPIO_CRH_MODE10_Pos) | // J   -> PA10
    (0x2 << GPIO_CRH_MODE11_Pos) | // BN  -> PA11
    (0x2 << GPIO_CRH_MODE12_Pos) | // B   -> PA12
    (0x2 << GPIO_CRH_MODE13_Pos) | // JN  -> PA13
    0
  );

  GPIOB->CRL = (GPIOB->CRL & ~(
    (GPIO_CRL_MODE0  | GPIO_CRL_CNF0) | // MA  -> PB0
    (GPIO_CRL_MODE1  | GPIO_CRL_CNF1) | // MB  -> PB1
    0
  )) | (
    (0x2 << GPIO_CRL_MODE0_Pos) | // MA  -> PB0
    (0x2 << GPIO_CRL_MODE1_Pos) | // MB  -> PB1
    0
  );

  GPIOB->CRH = (GPIOB->CRH & ~(
    (GPIO_CRH_MODE9  | GPIO_CRH_CNF9 ) | // PTT -> PB9
    (GPIO_CRH_MODE12 | GPIO_CRH_CNF12) | // A   -> PB12
    (GPIO_CRH_MODE13 | GPIO_CRH_CNF13) | // Y1N -> PB13
    (GPIO_CRH_MODE14 | GPIO_CRH_CNF14) | // Y3N -> PB14
    (GPIO_CRH_MODE15 | GPIO_CRH_CNF15) | // QN  -> PB15
    0
  )) | (
    (0x2 << GPIO_CRH_MODE9_Pos)  | // PTT -> PB9
    (0x2 << GPIO_CRH_MODE12_Pos) | // A   -> PB12
    (0x2 << GPIO_CRH_MODE13_Pos) | // Y1N -> PB13
    (0x2 << GPIO_CRH_MODE14_Pos) | // Y3N -> PB14
    (0x2 << GPIO_CRH_MODE15_Pos) | // QN  -> PB15
    0
  );

  GPIOC->CRH = (GPIOC->CRH & ~(
    (GPIO_CRH_MODE13  | GPIO_CRH_CNF13) | // Y7  -> PC13
    (GPIO_CRH_MODE14  | GPIO_CRH_CNF14) | // Y8  -> PC14
    (GPIO_CRH_MODE15  | GPIO_CRH_CNF15) | // AN  -> PC15
    0
  )) | (
    (0x2 << GPIO_CRH_MODE13_Pos) | // Y7  -> PC13
    (0x2 << GPIO_CRH_MODE14_Pos) | // Y8  -> PC14
    (0x2 << GPIO_CRH_MODE15_Pos) | // AN  -> PC15
    0
  );
}

extern uint32_t _sidata, _sdata, _edata, _sbss, _ebss;

void paint (char arr[], int len) {
  for (int i = 0; i < len; i +=3) {
    const char pos = arr[i];
    const uint16_t x = (pos % 10) * 24;
    const uint16_t y = (pos / 10) * 40;
    const uint16_t color = colors[(int)arr[i + 1]];
    const int digit = arr[i + 2];
    LCD_ShowChar(x, y, digit, 0, color);
  }
}

struct {
  char ON;
  char PTT;
  char PREAMP;
  char PWR;
  char BAND;
} state;

char stateInit [] = { // pos, clolor, digit
  /* ON     103 */  0,  8, 24,   1,   8, 23,                                                                          7, 15,  1,    8, 15,  0,    9, 15,  3,
  /* PTT        */ 10,  8, 25,   11,  8, 29,   12,  8, 29,
  /* PREAMP     */ 20,  8, 25,   21,  8, 27,   22,  8, 14,   23,  8, 10,   24,  8, 22,   25,  8, 25,
  /* PWR        */ 30,  8, 25,   31,  8, 32,   32,  8, 27,
  /* 01234567   */ 40, 15,  0,   41,  8,  1,   42,  8,  2,   43,  8,  3,   44,  8,  4,   45,  8,  5,   46,  8,  6,   47,  8,  7,
  /*     144000 */                             52,  0,  0,   53,  0,  0,   54,  2,  1,   55,  2,  4,   56,  2,  4,   57,  8,  0,   58,  8,  0,   59,  8,  0,
  /* I1     701 */ 60,  2, 18,   61,  2,  1,   67,  2,  8,   68,  2,  0,   69,  2,  1,
  /* U1     702 */ 70,  2, 30,   71,  2,  1,   77,  2,  8,   78,  2,  0,   79,  2,  2,
};

char stateON0 [] = {
  /* ON         */ 0,  8, 24,   1,  8, 23,
};

char stateON1 [] = {
  /* ON         */ 0, 15, 24,   1, 15, 23,
};

char statePTT0 [] = {
  /* PTT        */ 10, 8, 25,   11, 8, 29,   12, 8, 29,
};

char statePTT1 [] = {
  /* PTT        */ 10, 15, 25,   11, 15, 29,   12, 15, 29,
};

char statePREAMP0 [] = {
  /* PREAMP     */ 20, 8, 25,   21, 8, 27,   22, 8, 14,   23, 8, 10,   24, 8, 22,   25, 8, 25,
};

char statePREAMP1 [] = {
  /* PREAMP     */ 20, 15, 25,   21, 15, 27,   22, 15, 14,   23, 15, 10,   24, 15, 22,   25, 15, 25,
};

char statePWR0 [] = {
  /* PWR        */ 30, 8, 25,   31, 8, 32,   32, 8, 27,
};

char statePWR1 [] = {
  /* PWR        */ 30, 15, 25,   31, 15, 32,   32, 15, 27,
};

/* 01234567   */
   char stateBAND0 [] = { 40, 15,  0,   41,  8,  1,   42,  8,  2,   43,  8,  3,   44,  8,  4,   45,  8,  5,   46,  8,  6,   47,  8,  7,  /*     144000 */ 52, 0,  0,   53, 0,  0,   54, 2,  1,   55, 2,  4,   56, 2,  4,   57, 7,  0,   58, 7,  0,   59, 7,  0,};
   char stateBAND1 [] = { 40,  8,  0,   41, 15,  1,   42,  8,  2,   43,  8,  3,   44,  8,  4,   45,  8,  5,   46,  8,  6,   47,  8,  7,  /*     432000 */ 52, 0,  0,   53, 0,  0,   54, 2,  4,   55, 2,  3,   56, 2,  2,   57, 7,  0,   58, 7,  0,   59, 7,  0,};
   char stateBAND2 [] = { 40,  8,  0,   41,  8,  1,   42, 15,  2,   43,  8,  3,   44,  8,  4,   45,  8,  5,   46,  8,  6,   47,  8,  7,  /*    1296000 */ 52, 0,  0,   53, 2,  1,   54, 2,  2,   55, 2,  9,   56, 2,  6,   57, 7,  0,   58, 7,  0,   59, 7,  0,};
   char stateBAND3 [] = { 40,  8,  0,   41,  8,  1,   42,  8,  2,   43, 15,  3,   44,  8,  4,   45,  8,  5,   46,  8,  6,   47,  8,  7,  /*    2400000 */ 52, 0,  0,   53, 2,  2,   54, 2,  4,   55, 2,  0,   56, 2,  0,   57, 7,  0,   58, 7,  0,   59, 7,  0,};

   char stateBAND4 [] = { 40,  8,  0,   41,  8,  1,   42,  8,  2,   43,  8,  3,   44, 15,  4,   45,  8,  5,   46,  8,  6,   47,  8,  7,  /*       SATA */ 52, 0,  0,   53, 0,  0,   54, 0,  0,   55, 0,  0,   56, 2, 28,   57, 2, 10,   58, 2, 29,   59, 3, 10,};
   char stateBAND5 [] = { 40,  8,  0,   41,  8,  1,   42,  8,  2,   43,  8,  3,   44,  8,  4,   45, 15,  5,   46,  8,  6,   47,  8,  7,  /*       SATB */ 52, 0,  0,   53, 0,  0,   54, 0,  0,   55, 0,  0,   56, 2, 28,   57, 2, 10,   58, 2, 29,   59, 3, 11,};
   char stateBAND6 [] = { 40,  8,  0,   41,  8,  1,   42,  8,  2,   43,  8,  3,   44,  8,  4,   45,  8,  5,   46, 15,  6,   47,  8,  7,  /*       SATJ */ 52, 0,  0,   53, 0,  0,   54, 0,  0,   55, 0,  0,   56, 2, 28,   57, 2, 10,   58, 2, 29,   59, 3, 19,};
   char stateBAND7 [] = { 40,  8,  0,   41,  8,  1,   42,  8,  2,   43,  8,  3,   44,  8,  4,   45,  8,  5,   46,  8,  6,   47, 15,  7,  /*       SATQ */ 52, 0,  0,   53, 0,  0,   54, 0,  0,   55, 0,  0,   56, 2, 28,   57, 2, 10,   58, 2, 29,   59, 3, 26,};

// void delay_ms(uint32_t d) {
//   for (uint32_t i = 0; i < (d * 1300); i++) {
//     __asm__( "nop; nop; nop" );
//   }
// }

void onEnter0 (void) { state.BAND = 0; paint(stateBAND0, sizeof(stateBAND0)); MA_0 MB_0  Y1_0 Y1N1 Y2_0 Y3_1 Y3N0 Y4_0 Y5_0 Y5N1 A__0 AN_1 B__0 BN_1 J__0 JN_1 Q__1 QN_0 }
void onEnter1 (void) { state.BAND = 1; paint(stateBAND1, sizeof(stateBAND1)); MA_0 MB_1  Y1_1 Y1N0 Y2_0 Y3_0 Y3N1 Y4_0 Y5_0 Y5N1 A__0 AN_1 B__0 BN_1 J__0 JN_1 Q__1 QN_0 }
void onEnter2 (void) { state.BAND = 2; paint(stateBAND2, sizeof(stateBAND2)); MA_1 MB_0  Y1_0 Y1N1 Y2_1 Y3_0 Y3N1 Y4_0 Y5_0 Y5N1 A__0 AN_1 B__0 BN_1 J__0 JN_1 Q__1 QN_0 }
void onEnter3 (void) { state.BAND = 3; paint(stateBAND3, sizeof(stateBAND3)); MA_1 MB_1  Y1_0 Y1N1 Y2_0 Y3_0 Y3N1 Y4_1 Y5_0 Y5N1 A__0 AN_1 B__0 BN_1 J__0 JN_1 Q__1 QN_0 }

void onEnter4 (void) { state.BAND = 4; paint(stateBAND4, sizeof(stateBAND4)); MA_0 MB_0  Y1_0 Y1N1 Y2_0 Y3_1 Y3N0 Y4_0 Y5_1 Y5N0 A__1 AN_0 B__0 BN_1 J__0 JN_1 Q__1 QN_0 }
void onEnter5 (void) { state.BAND = 5; paint(stateBAND5, sizeof(stateBAND5)); MA_0 MB_0  Y1_1 Y1N0 Y2_0 Y3_0 Y3N1 Y4_0 Y5_1 Y5N0 A__0 AN_1 B__1 BN_0 J__0 JN_1 Q__1 QN_0 }
void onEnter6 (void) { state.BAND = 6; paint(stateBAND6, sizeof(stateBAND6)); MA_0 MB_0  Y1_0 Y1N1 Y2_0 Y3_1 Y3N0 Y4_0 Y5_1 Y5N0 A__0 AN_1 B__0 BN_1 J__1 JN_0 Q__1 QN_0 }
void onEnter7 (void) { state.BAND = 7; paint(stateBAND7, sizeof(stateBAND7)); MA_0 MB_0  Y1_0 Y1N1 Y2_0 Y3_0 Y3N1 Y4_1 Y5_0 Y5N1 A__0 AN_1 B__0 BN_1 J__0 JN_1 Q__0 QN_1 }


void setup_clock (void) {
  // HXTAL = 8MHz
  // HXTAL = 14.7456 MHz
  RCC->CR |= (0x1 << 16) /* RCU_CTL_HXTALEN */;
  while((RCC->CR & (0x1 << 17)/* RCU_CTL_HXTALSTB */) == 0U) {}

  /* AHB = SYSCLK */
  // RCC->CFGR |= (0 << 4); RCU_AHB_CKSYS_DIV1
  /* APB2 = AHB/1 */
  // RCC->CFGR |= (0 << 11); RCU_APB2_CKAHB_DIV1;
  /* APB1 = AHB/2 */

  RCC->CFGR |= (0x4 << 8); // RCU_APB1_CKAHB_DIV2;

  /* CK_PLL = (CK_PREDIV0) * 27 = 108 MHz */
  /* CK_PLL = (CK_PREDIV0) * 14 = 103.2192 MHz */
  RCC->CFGR &= ~(
    (0x1 << 29) | // RCU_CFG0_PLLMF_4
    (0xf << 18)   // RCU_CFG0_PLLMF |
  );
  RCC->CFGR |=  (
    (0x1 << 16) | // RCU_PLLSRC_HXTAL
    // (0x1 << 29) | (0xa << 18)// RCU_PLL_MUL27
    (0x0 << 29) | (0xc << 18)// RCU_PLL_MUL14
  );
  RCC->CFGR1 &= ~(
    (0x1 << 16) | // RCU_CFG1_PREDV0SEL |
    (0xf << 4 ) | // RCU_CFG1_PREDV1 |
    (0xf << 8 ) | // RCU_CFG1_PLL1MF |
    (0xf << 0)    // RCU_CFG1_PREDV0
  );
	RCC->CFGR1 |= (
    (0x0 << 16) | // RCU_PREDV0SRC_HXTAL |
    (0x1 << 0 ) | // RCU_PREDV0_DIV2 |
    (0x1 << 4 ) | // RCU_PREDV1_DIV2 |
    // (0xf << 8 ) | // RCU_PLL1_MUL20 |
    // (0xf << 12)   // RCU_PLL2_MUL20
    (0xc << 8 ) | // RCU_PLL1_MUL14 |
    (0xc << 12)   // RCU_PLL2_MUL14
  );

  /* enable PLL1 */
  RCC->CR |= (0x1 << 26); // RCU_CTL_PLL1EN;
  /* wait till PLL1 is ready */
  while (0U == (RCC->CR & (0x1 << 27) /* RCU_CTL_PLL1STB */)) {}
  /* enable PLL2 */
  RCC->CR |= (0x1 << 28); // RCU_CTL_PLL2EN;
  /* wait till PLL1 is ready */
  while (0U == (RCC->CR & (0x1 << 29) /* RCU_CTL_PLL2STB */)) {}

  /* enable PLL */
  RCC->CR |= (0x1 << 24); // RCU_CTL_PLLEN;

  /* wait until PLL is stable */
  while (0U == (RCC->CR & (0x1 < 25) /* RCU_CTL_PLLSTB */)) {}

  /* select PLL as system clock */
  RCC->CFGR &= ~(0x3 << 0); // RCU_CFG0_SCS;
  RCC->CFGR |= (0x2 << 0); // RCU_CKSYSSRC_PLL;

  /* wait until PLL is selected as system clock */
  while ((0x2 << 2) != (RCC->CFGR & (0x3 << 2) /* RCU_SCSS_PLL */)) {}
}


int is_domino_valid (uint32_t x) {
  return ((x & 0x00ff00ff) ^ ((x >> 8) & 0x00ff00ff)) == 0x00ff00ff;
}

void handle_ir(void) {
  // PB7 <- IR
  GPIOB->CRL = (GPIOB->CRL & ~(
    (GPIO_CRL_MODE7 | GPIO_CRL_CNF7)
  )) | (
    (0x0 << GPIO_CRL_MODE7_Pos) | (0x2 << GPIO_CRL_CNF7_Pos)
  );
  // watch IR
  uint32_t code;

  uint32_t t0, t1, delta;

  paint(stateInit, sizeof(stateInit));
  onEnter0();

  while (1) {

    while ( ((GPIOB->IDR >> 7) & 1)); // . -> *
    t0 = mtime_lo();

    while (!((GPIOB->IDR >> 7) & 1)); // * -> .
    t1 = mtime_lo();
    delta = t1 - t0;
    if (delta < (TIMER_FREQ / 1000 * 8 )) goto restart; // error < 6ms (9ms)
    if (delta > (TIMER_FREQ / 1000 * 10)) goto restart; // error > 12ms (9ms)
    t0 = t1;

    while ( ((GPIOB->IDR >> 7) & 1)); // . -> *

    t1 = mtime_lo();
    delta = t1 - t0;
    if (delta < (TIMER_FREQ / (1000))) goto restart; // error < 1ms
    if (delta < (TIMER_FREQ / (10000 / 35))) goto restart; // repeat

    code = 0;
    for (int i = 0; i < 32; i++) {
      t0 = mtime_lo();
      while(!((GPIOB->IDR >> 7) & 1)); // * -> .
      t1 = mtime_lo();
      delta = t1 - t0;
      if (delta < (TIMER_FREQ / (10000 / 3))) goto restart; // < 0.3 (0.562ms)
      if (delta > (TIMER_FREQ / (1000     ))) goto restart; // > 1   (0.562ms)
      t0 = t1;
      while( (GPIOB->IDR >> 7) & 1);    // . -> *
      t1 = mtime_lo();
      delta = t1 - t0;
      if (delta < (TIMER_FREQ / (10000 / 3))) goto restart; // < 0.3ms
      if (delta > (TIMER_FREQ / (1000 / 3))) goto restart;  // > 3ms
      code = code << 1;
      if (delta > (TIMER_FREQ / (10000 / 17))) code |= 1; // > 1.7ms
    }

    if (!is_domino_valid(code)) goto restart;

    code = code & 0xffff;

    for (int i = 0; i < 4; i++) {
      LCD_ShowChar(
        (i + 6) * 24,
        0,
        (code >> ((3 - i) * 4)) & 0xf,
        0,
        colors[7]
      );
    }

    switch(code) {
      case 0x10EF: // ON/OFF
        if (state.ON) {
          ON_0
          state.ON = 0;
          paint(stateON0, sizeof(stateON0));
        } else {
          ON_1
          state.ON = 1;
          paint(stateON1, sizeof(stateON1));
        }
        break;
      case 0x22DD: // OK
        if (state.PTT) {
          PTT1 PTN0
          state.PTT = 0;
          paint(statePTT0, sizeof(statePTT0));
        } else {
          PTT0 PTN1
          state.PTT = 1;
          paint(statePTT1, sizeof(statePTT1));
        }
        break;
      case 0xCA35: // LIST
        if (state.PREAMP) {
          Y7_1
          state.PREAMP = 0;
          paint(statePREAMP0, sizeof(statePREAMP0));
        } else {
          Y7_0
          state.PREAMP = 1;
          paint(statePREAMP1, sizeof(statePREAMP1));
        }
        break;
      case 0x58A7: // Q.VIEW
        if (state.PWR) {
          Y8_1
          state.PWR = 0;
          paint(statePWR0, sizeof(statePWR0));
        } else {
          Y8_0
          state.PWR = 1;
          paint(statePWR1, sizeof(statePWR1));
        }
        break;
      case 0x08F7: onEnter0(); break;
      case 0x8877: onEnter1(); break;
      case 0x48B7: onEnter2(); break;
      case 0xC837: onEnter3(); break;
      case 0x28D7: onEnter4(); break;
      case 0xA857: onEnter5(); break;
      case 0x6897: onEnter6(); break;
      case 0xE817: onEnter7(); break;
      // case 0x18E7: onEnter8(); break;
      // case 0x9867: onEnter9(); break;
      case 0x00FF: // PgUp
        switch(state.BAND) {
          case  0: onEnter1(); break;
          case  1: onEnter2(); break;
          case  2: onEnter3(); break;
          case  3: onEnter4(); break;
          case  4: onEnter5(); break;
          case  5: onEnter6(); break;
          case  6: onEnter7(); break;
          default: onEnter0(); break;
          // case  8: onEnter9(); break;
          // default: onEnter0(); break;
        }
        break;
      case 0x807F: // PgDown
        switch(state.BAND) {
          case  0: onEnter7(); break;
          case  1: onEnter0(); break;
          case  2: onEnter1(); break;
          case  3: onEnter2(); break;
          case  4: onEnter3(); break;
          case  5: onEnter4(); break;
          case  6: onEnter5(); break;
          default: onEnter6(); break;
          // case  8: onEnter7(); break;
          // default: onEnter8(); break;
        }
        break;

    }

    restart:
    while(!((GPIOB->IDR >> 7) & 1)); // * -> .
    code = 0;
  }
}



int main () {
  memcpy( &_sdata, &_sidata, ( ( void* )&_edata - ( void* )&_sdata ) ); // copy Flash -> RAM
  memset( &_sbss, 0x00, ( ( void* )&_ebss - ( void* )&_sbss ) ); // Clear .bss RAM

  setup_clock();

  port_init();
  mapc_init();

  lcd_init();
  // LED_K_OFF


  // // PA14 <- Pluto GPIO0
  // // PA15 <- PTT_EXT ???
  // GPIOA->CRH = GPIOA->CRH
  // & (~(
  //   (GPIO_CRH_MODE14 | GPIO_CRH_CNF14) |
  //   (GPIO_CRH_MODE15 | GPIO_CRH_CNF15)
  // ) | (
  //   // 0x0 << 214U
  //   (0x0 << GPIO_CRH_MODE14_Pos) | (0x2 << GPIO_CRH_CNF14_Pos) |
  //   (0x0 << GPIO_CRH_MODE15_Pos) | (0x2 << GPIO_CRH_CNF15_Pos)
  // ));


  // setRotation(4);
  lcd_fill(0, 0, 240, 320, 0x000);

  // int code = 987654321;

  // // LED_K_ON

  // for (int i = 9; i >= 0; i--) {
  //   const int digit = code % 10;
  //   LCD_ShowChar(i * 24, 0, digit, 0, 0x0ee0);
  //   code = code / 10;
  //   if (code == 0) {
  //     break;
  //   }
  // }

  // Initial state

  handle_ir();

  // int state = 0;
  // while(1) {
  //   const int next = (GPIOA->IDR >> 14) & 3;
  //   if (next != state) {
  //     state = next;
  //     LCD_ShowChar(240 - 24, 320 - 40, state, 0, 0xffff);
  //   }
  // }

  return 0;
}
