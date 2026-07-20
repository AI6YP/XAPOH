#ifndef _FUNCONFIG_H
#define _FUNCONFIG_H

// Low-noise: chip sits inside a sensitive RF circuit.
// Minimal clock, no PLL. HSI/16 = 1.5 MHz (same as sw-v3 expander).
#define FUNCONF_USE_HSI 1 // internal 24MHz RC oscillator
#define FUNCONF_USE_PLL 0 // no PLL
#define FUNCONF_SYSTEM_CORE_CLOCK 1500000

// No debug-printf UART — PD1 is used as USART1 RX for the link, not for printf.
#define FUNCONF_USE_DEBUGPRINTF 0
#define FUNCONF_USE_UARTPRINTF  0

#define CH32V003 1

#endif
