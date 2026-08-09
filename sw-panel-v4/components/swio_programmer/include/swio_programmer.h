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
// No external pull-up resistor needed: reads use a glitch+release technique
// (brief D_HI to charge the line, then D_REL so the chip drives its response).
// The ESP's internal pull-up + the CH32's internal pull-up hold the line high.
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

// Flash `bin`/`len` into a CH32V003 flash (@0x08000000) on the given GPIO.
// Runs the full sequence: pad config -> SDI wakeup -> connect -> halt -> unlock
// -> erase -> program -> verify -> reboot. Returns ESP_OK on verified success.
esp_err_t swio_flash_image(int gpio, const uint8_t *bin, size_t len);

// Bring-up aid: scope-visible pin diagnostics (blink, 50kHz toggle, read
// monitor, SWIO traffic sampler). Call from the S0/S1/S2 USB command.
void swio_pin_selftest(int gpio);

// Bring-up aid for the "reflash a running chip" problem: tries to connect on
// `gpio` continuously for `seconds`. Power-cycle the CH32 mid-probe to catch
// the startup window. Returns ESP_OK if the chip answered at least once.
esp_err_t swio_listen(int gpio, int seconds);

#ifdef __cplusplus
}
#endif
