// SWIO (WCH single-wire RV-SWD) flasher for CH32V003, running on the ESP32-C6.
//
// Fresh in-repo port: the low-level single-wire bit-bang follows cnlohr's
// esp32s2-cookbook (ch32v003_swio.h / bitbang_rvswdio.h, SWIO/opmode-0 path),
// rewritten for the RISC-V C6 (REG-level GPIO, RISC-V PrecDelay). The Debug-Module
// flash algorithm mirrors the validated web3/lib/wchlink.js.
//
// CH32V003 is pure single-wire: only the SWIO/data line is used (no SWCLK).
// Each expander is on its own ESP GPIO; flash them one at a time.
//
// Use ONLY in program mode (board jumper at 3.3 V–3.3 V). The pin is shared with
// the runtime UART TX — the caller must stop UART on this gpio first and restore
// it after (see swio_programmer per-flash interlock in the app).

#pragma once
#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Bit-bang timing coefficient. ~1 µs base pulse on a 160 MHz C6.
// MUST be calibrated on hardware (scope or program+verify success). Lower = faster.
#ifndef SWIO_T1COEFF
#define SWIO_T1COEFF 80
#endif

// Flash `bin`/`len` into a CH32V003 flash (@0x08000000) on the given GPIO.
// Runs the full sequence: pad config -> SWIO connect -> halt -> unlock -> erase ->
// program -> verify -> reboot. Returns ESP_OK on verified success.
esp_err_t swio_flash_image(int gpio, const uint8_t *bin, size_t len);

// Bring-up aid: scope-visible ~50 kHz square wave on `gpio` for ~1 s. Confirms
// the ESP drives the pad. Call from the F0 handler in place of swio_flash_image.
void swio_pin_selftest(int gpio);

#ifdef __cplusplus
}
#endif
