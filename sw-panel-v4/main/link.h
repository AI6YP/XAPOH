// v4 expander link: single HW UART1, GPIO-matrix-switched across the 3
// point-to-point wires to the CH32V003 expanders (gpio0/1/2). Replaces the
// bit-bang soft_uart (A8 revised): TX-only + one-target-per-change ⇒ one HW UART.
// The HW shifter is jitter-proof (no critical section, unlike bit-bang).
//
// The same gpio is SWIO in program mode; link_flash_begin/end hand the raw pin
// to the SWIO programmer and restore the idle-high UART line afterwards, and
// serialize against concurrent link_send (flash interlock).
#pragma once
#include <stdint.h>
#include <stddef.h>

// Install UART1 (4800 8N1) and park the 3 links as idle-high GPIO outputs.
void link_init(void);

// Route U1TXD to link `idx` (0..2) and send `len` bytes (8N1, LSB first).
void link_send(int idx, const uint8_t *buf, size_t len);

// Physical gpio backing link `idx` (for the SWIO programmer).
int link_gpio_num(int idx);

// Flash interlock: take the link and detach U1TXD so SWIO owns the raw pin.
// Must be paired with link_flash_end(idx).
void link_flash_begin(int idx);

// Re-park the pin idle-high and release the link (rebinds to U1TXD on next send).
void link_flash_end(int idx);
