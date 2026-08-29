// XAPOH port-expander firmware v4 (CH32V003).
//
// Replaces the v3 I2C slave with a one-wire UART receiver on PD1 (the SWIO pin,
// chip pin 18). The ESP32-C6 sends a fixed 5-byte full-state frame; this firmware
// drives 17 panel outputs accordingly. PD1 is flashed via SWIO by the ESP when the
// board is in program mode (3.3 V), and used as USART1 RX in operation (5 V).
//
// Link wire / mode notes:
//   - USART1 RX is remapped to PD1 (AFIO PCFR1 USART1 remap = 01: RX/PD1, TX/PD0).
//   - Only RX is enabled; PD0 is a driven output (no USART TX function used, so
//     remapping TX onto PD0 is safe to repurpose).
//   - Baud 4800 @ 1.5 MHz core. UART bit ~208 us vs SWIO bit ~1 us => the always-on
//     debug tap can never mistake UART traffic for a programming transaction.
//
// RF discipline (chip lives inside sensitive RF): 1.5 MHz / no PLL, sleep in __WFI
// and wake only on RXNE, 2 MHz output slew, drive only pins that actually change.

#include "ch32fun.h"
#include <stdint.h>

// --- wire protocol -----------------------------------------------------------
// Frame = 0xAA | b0 | b1 | b2 | (b0^b1^b2).  Full-state push, idempotent.
//   b0[0..7] -> PD4, PD5, PD6, PD7, PA1, PA2, PD0, PC0
//   b1[0..7] -> PC1, PC2, PC3, PC4, PC5, PC6, PC7, PD2,
//   b2[0]    -> PD3
#define FRAME_SYNC 0xAA
#define BAUD 4800

// 17 active outputs, packed bit i (i=0..16) -> outputs[i].
static const uint8_t outputs[17] = {
  PD4, PD5, PD6, PD7, PA1, PA2, PD0, PC0, // b0[0..7]
  PC1, PC2, PC3, PC4, PC5, PC6, PC7, PD2, // b1[0..7]
  PD3                                     // b2[0]
};

static uint32_t out_shadow; // last applied packed state (bit i = pin state)

// Drive only the pins whose bit changed since last apply (less switching noise).
static void apply_outputs(uint32_t packed) {
  uint32_t diff = packed ^ out_shadow;
  if (!diff) return;
  for (uint8_t i = 0; i < 17; i++) {
    if (diff & (1u << i)) {
      funDigitalWrite(outputs[i], (packed >> i) & 1u);
    }
  }
  out_shadow = packed;
}

// --- frame receiver (RXNE interrupt) -----------------------------------------
static volatile uint32_t rx_pending; // newest decoded packed state
static volatile uint8_t  rx_ready;   // set when a valid frame arrived

void USART1_IRQHandler(void) __attribute__((interrupt));
void USART1_IRQHandler(void) {
  static uint8_t st, b0, b1, b2;
  // Reading STATR then DATAR clears RXNE and any overrun (ORE) condition.
  (void)USART1->STATR;
  uint8_t d = (uint8_t)USART1->DATAR;

  switch (st) {
    case 0: if (d == FRAME_SYNC) st = 1; break;     // resync on 0xAA
    case 1: b0 = d; st = 2; break;
    case 2: b1 = d; st = 3; break;
    case 3: b2 = d; st = 4; break;
    case 4:
      if (d == (uint8_t)(b0 ^ b1 ^ b2)) {
        rx_pending = (uint32_t)(b0) | ((uint32_t)(b1) << 8) | ((uint32_t)(b2) << 16);
        rx_ready = 1;
      }
      st = 0;
      break;
    default: st = 0; break;
  }
}

// --- setup -------------------------------------------------------------------
static void gpio_init(void) {
  funGpioInitAll();

  // 17 panel outputs: push-pull, 2 MHz slew (low EMI), start low.
  for (uint8_t i = 0; i < 17; i++) {
    funPinMode(outputs[i], GPIO_Speed_2MHz | GPIO_CNF_OUT_PP);
    funDigitalWrite(outputs[i], 0);
  }
  out_shadow = 0;

  // PD1 = USART1 RX (link wire): input, pull-up so the idle line reads high.
  funPinMode(PD1, GPIO_CNF_IN_PUPD); funDigitalWrite(PD1, 1);
}

static void usart_rx_init(void) {
  // AFIO clock is already enabled by funGpioInitAll(); add the USART1 clock.
  RCC->APB2PCENR |= RCC_APB2Periph_USART1;

  // USART1 remap = 01 -> RX on PD1, TX on PD0 (TX unused, TE stays off).
  AFIO->PCFR1 = (AFIO->PCFR1 & ~AFIO_PCFR1_USART1_REMAP_1) | AFIO_PCFR1_USART1_REMAP;

  // 8N1, BRR = Fclk/baud (= 312 @ 1.5 MHz/4800 -> 4808 baud, +0.16%).
  USART1->BRR = FUNCONF_SYSTEM_CORE_CLOCK / BAUD;
  USART1->CTLR1 = USART_CTLR1_RE | USART_CTLR1_RXNEIE | USART_CTLR1_UE;

  NVIC_EnableIRQ(USART1_IRQn);
}

int main(void) {
  SystemInit();

  RCC->CFGR0 = RCC_HPRE_DIV16; // HCLK = SYSCLK/16 = 1.5 MHz
  // Clear PLL/HSE but KEEP the HSITRIM field: writing bare RCC_HSION (0x01) zeroes
  // HSITRIM (default 16, ~0.34%/step) -> ~5% HSI detune -> UART framing fails.
  RCC->CTLR &= ~(RCC_PLLON | RCC_HSEON);

  gpio_init();
  usart_rx_init();

  for (;;) {
    __WFI(); // sleep; wake on RXNE interrupt
    if (rx_ready) {
      rx_ready = 0;
      apply_outputs(rx_pending);
    }
  }
}
