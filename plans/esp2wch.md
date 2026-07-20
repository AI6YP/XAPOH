### What we have now

* main C control software in XAPOH/sw-panel-v3/ folder running on esp32c6 and controlling XAPOH device via 3 I2C port expanders. via HW I2C master interface ports 3,4.
* i2c port expander code in XAPOH/sw-v3/expander running on 3 CH32V003 devices connected by I2C, via ports PC1/PC2 (I2C slave mode) controlled by esp32c6.
* web interface tool web3 with 2 tabs:
  - bridge: programming esp32c6 control software binary image and WiFi configuration.
  - expander: programming port expander device binary images via WCH-LinkE programmer -> PD1/SWIO pin 18.

### What we want to build

* New main C control software in XAPOH/sw-panel-v4/ folder running on esp32c6
* controlling XAPOH device via 3 CH32V003 devices, connected via 3 point-to-point wires from esp32c3 ports 0,1,2 to SWIO port of each of 3 CH32V003 devices.
* esp32c6 will act a CH32V003 programmer via SWIO pin to flash FW similar to ch32fun/minichlink code.
* CH32V003 device software in XAPOH/sw-v4/ will use SWIO pin 18 in UART RX mode to recieve port values from esp32c6.

---

## Architecture & Implementation Plan (v4)

### Decisions locked (2026-06-30)
- Runtime data flow: **TX-only**, ESP32-C6 → CH32V003. CH32 never replies. (matches v3, where `onWrite` only drives outputs)
- CH32V003 core clock: **keep 1.5 MHz** (HSI/16, as v3 `funconfig.h`).
- Session scope: architecture + this written plan. No code yet.
- SWIO programmer source: port cnlohr `esp32s2-cookbook/ch32v003programmer`; fall back to fully in-repo port if cleaner.

### The core idea: one wire, two roles
Each CH32V003 PD1 (chip pin 18) is wired point-to-point to one ESP32-C6 GPIO
(gpio0/1/2). That single wire is time-shared:

1. **Flash time** — ESP32-C6 bit-bangs WCH single-wire RV-SWD (SWIO) on the pin
   to halt the core and program flash, replacing the external WCH-LinkE.
2. **Run time** — ESP32-C6 sends a UART byte stream; the CH32 reads it on the
   same pin via **USART1 RX remapped to PD1**.

Feasibility confirmed: CH32V003 `AFIO->PCFR1` USART1 remap = `01`
(`AFIO_PCFR1_USART1_REMAP` set, `..._REMAP_1` clear) maps **USART1_RX → PD1**
(TX → PD0). See `cnlohr/ch32fun/ch32fun/ch32v003hw.h:1566`.

The pin reuse only works because at run time the CH32 keeps PD1 as an **input**
(USART RX), so it never fights the line, and the QingKe debug module stays alive
(do NOT disable SWD via option bytes), so the ESP can re-enter debug to reflash.

### Operating modes (set by a physical jumper)
The board has **two mutually-exclusive modes, selected by a physical jumper
before power-up** — the ESP does NOT switch them:

| | jumper / power | ESP32-C6 link | host | CH32 flashing | ESP SWIO |
|---|---|---|---|---|---|
| **Program** | 3.3 V–3.3 V | USB → host PC | web4 over USB (WebSerial) | yes, on web4 button | yes |
| **Operation** | 3.3 V–5 V | no USB; WiFi | WiFi WS (hosted UI/app) | never | **never** |

Consequences of the jumper model:
- **No ESP-controlled rail switch** — removed. Mode = physical jumper + power.
  Powering up in Program mode gives the clean 3.3 V state for SWIO.
- **Transport splits by mode.** Program: web4 ↔ ESP over **USB serial**; the
  "flash expander N" command only exists on USB. Operation: panel control over
  **WiFi WS**. So "ESP never performs SWIO in operation" falls out for free — with
  no USB host there is no flash command to act on.
- **Mode detection — none needed (command-driven).** The ESP always boots the
  operational role (WiFi + UART TX). It also listens on USB-serial; a flash command
  from web4 is the *only* thing that starts SWIO. In Operation mode there is no USB
  host, so no command arrives → no SWIO. No boot-time role branch, no strap.
- The "reflash a *running* chip without NRST" risk still applies: in Program mode
  the CH32 powers up and starts its firmware (remaps PD1→USART RX) before the ESP
  halts it. Re-entry relies on the always-on QingKe debug tap (cnlohr does this
  routinely) — still a test item.

### Link-wire levels & pull-up (hardware)

| mode | ESP32-C6 | CH32V003 | link wire |
|---|---|---|---|
| **program (SWIO)** | 3.3 V | **3.3 V** | both 3.3 V — matched, bidir SWIO clean |
| **operation (UART)** | 3.3 V | **5 V** | mismatch: 3.3 V driver → 5 V receiver |

Operation-mode level check — **no level shifter needed.** CH32V003 input-high
threshold is `VIH = 0.22·(Vdd − 2.7) + 1.55`:
- Vdd = 5 V → VIH = **2.06 V**. ESP 3.3 V push-pull high clears it with margin.
- Vdd = 3.3 V (program) → VIH = **1.68 V**. Also fine.

So ESP→CH32 (3.3 V → 5 V) is reliable directly. Direction is one-way (CH32 PD1
stays an input), so the **ESP never sees 5 V** — *provided the link net's pull-up
is referenced to 3.3 V, not 5 V*, and CH32 firmware never drives PD1.

Pull-up (decided): use the **internal pull-up** via the `R_GLITCH_HIGH` technique
from the cookbook SWIO code (no external 10k). Requires setting the gpio drive to
**5 mA** so the brief active-high glitch works. Only used during SWIO (program
mode, both 3.3 V). Operation UART is push-pull, no pull-up needed; the line never
sits at 5 V on the ESP pin.

### v3 → v4 delta
| | v3 | v4 |
|---|---|---|
| ESP↔CH32 transport | HW I2C master (gpio3/4), addr 0x0A/0B/0C | 3× point-to-point single wire (gpio0/1/2) |
| CH32 runtime input | I2C slave on PC1/PC2 | USART1 RX on PD1 (SWIO), 1-way |
| CH32 addressing | PD0 strap → 0xa/0xb/0xc | none — point-to-point, identical FW on all 3 |
| CH32 flashing | external WCH-LinkE on PD1 | ESP32-C6 bit-bangs SWIO on PD1 |
| ESP role | WiFi + WS + I2C + RGB | WiFi + WS + SWIO programmer + 3× UART TX + RGB |
| `i2c_slave.h` | used | dropped |

The web/WS framing (`buf[0]==255` → RGB strip; else → port values) and the
register→GPIO bit mapping (`onWrite` in `sw-v3/expander/main.c`) carry over
unchanged in spirit.

---

### Component A — `sw-v4/` : CH32V003 firmware (UART RX expander)
ch32fun project, Makefile modeled on `sw-v3/expander/Makefile`. `funconfig.h`
keeps 1.5 MHz HSI.

**Low-noise / RF discipline (hard requirement — chip sits inside sensitive RF).**
Goal: minimal spectral emission. Rules:
- **Lowest clock, no PLL.** Keep HSI/16 = 1.5 MHz (`FUNCONF_USE_PLL 0`). Do not
  enable PLL or HSE. 1.5 MHz is the floor that still supports the chosen UART baud
  (19200/9600); don't go lower than the baud needs.
- **Sleep by default.** Core in `__WFI` (or `__WFE`); wake only on USART1 RXNE
  interrupt, process the frame, apply outputs, sleep again. No polling loop, no
  SysTick, no periodic timers.
- **Slow slew on outputs.** Use the **2 MHz** output mode (`GPIO_Speed_2MHz`),
  not v3's 50 MHz — slower edges = far less HF harmonic content.
- **Toggle less.** Only write GPIO pins whose bit actually changed vs last frame
  (diff against shadow copy); never re-drive all 17 each update. No idle blinking.
- **Gate unused clocks.** Enable only GPIO ports in use + USART1 + AFIO; leave
  all other peripheral clocks (I2C, SPI, TIMx, ADC) off.
- **Keep the link static between updates** (see Component C: ESP sends only on
  change, line idle-high). The data wire is the main in-band noise source during
  operation; event-driven traffic minimizes its duty cycle.

- GPIO outputs: **14 active now** (same set as v3):
  PD4,PD5,PD6,PD7,PA1,PA2,PC0,PC3,PC4,PC5,PC6,PC7,PD2,PD3.
- The 3 pins freed by dropping I2C — **PD0** (was addr strap), **PC1**/**PC2**
  (was I2C) — are **reserved as inputs for now** (enable as outputs later as
  needed). Configure as input with a defined pull (avoid floating → RF noise).
- Reserved: **PD1** = SWIO / USART1 RX (the link wire).
- Drop I2C / `i2c_slave.h` / PD0 strap entirely.
- Configure USART1: enable AFIO, set `PCFR1` USART1 remap=01 (RX→PD1), PD1 as
  input-floating/pull-up, USART1 RX-only, RXNE interrupt.
- Baud: **4800** (decided). Clean at 1.5 MHz: USARTDIV 19.5 (eff 4808, +0.16%),
  well within ±2.5%. Only a 5-byte frame ships per change, so speed is irrelevant;
  lower = less noise + bigger SWIO-timing separation. Plenty of core cycles between
  bytes for the trivial ISR.
- State = **3 bytes** (fixed format; 14 bits active now, b2 reserved). Bit map:
  b0[0..6]=PD4,PD5,PD6,PD7,PA1,PA2,PC0; b1[0..6]=PC3,PC4,PC5,PC6,PC7,PD2,PD3
  (both as v3). b2[0..2] reserved for PD0/PC1/PC2 — **ignored** until those pins
  are switched to outputs (keeps the wire protocol stable when they're enabled).
- Framing: **full-state push** (no addressing) — send the whole 3-byte snapshot on
  any change. Frame = `0xAA (sync) | b0 | b1 | b2 | (b0^b1^b2) (checksum)` = 5 bytes.
  ISR collects 5 bytes, validates sync + checksum, then applies the bit→GPIO map
  (diff against shadow, drive only changed pins). Bad frame → resync on next 0xAA.
  Idempotent: a dropped frame self-heals on the next change push.
- Must **not** disable debug (no option-byte SWD-off) so the ESP can reflash.

### UART vs SWIO non-conflict (critical — must not accidentally halt the chip)
The QingKe debug tap on PD1 is **always live** (that is exactly what lets the ESP
reflash a running chip). So UART bytes physically reach the debug FSM. We must
guarantee UART traffic can never be mistaken for a programming transaction.

Why it's safe — two independent margins:
1. **Timing separation (~100×).** SWIO encodes a bit by the *width of the low
   pulse* at ≈1 µs (`t1coeff`): short-low = 1, long-low (4×) = 0; line idles high.
   A UART bit at 9600 is 104 µs (208 µs at 4800). Every UART low is ~100–800× too
   long to read as any valid SWIO bit, so the debug FSM treats it as idle/reset
   noise, never a clocked bit.
2. **Halt needs a complete valid DMI write** — start + 7-bit address + 32-bit
   data + parity at SWIO timing. UART framing physically cannot assemble that. The
   worst pattern (a 0x00 byte ≈ 0.9 ms continuous low = a UART BREAK) only pokes
   the debug-FSM reset; the core keeps running, it does **not** halt.

Design rules that lock this in:
- Keep baud low (9600/4800) — preserve the timing gap; never raise it toward µs.
- The ESP emits **only** UART (104 µs bits, push-pull) in operation and **only**
  SWIO (1 µs pulses) in program mode — never both; mode is interlocked + the rail
  switch power-cycles between them (Power domains).
- Never run the SWIO debug-entry "song and dance" during operation.
- Line idles high in both modes (UART idle = SWIO idle), so transitions out of
  idle are always unambiguous to each receiver.
- Optional belt-and-suspenders: avoid all-zero payload bytes (e.g. keep b2 sync
  bits set, or invert) to eliminate the long-BREAK pattern entirely. Cheap; do it
  if P2 testing shows any FSM disturbance.
- **Verification (P3):** stream worst-case UART (incl. 0x00 floods) at full rate
  to a *running* CH32 and confirm via its outputs that it never halts/resets.

### Component B — `sw-panel-v4/components/swio_programmer/` : SWIO flasher on C6
**Fresh in-repo C port** (decided A10) — no dependency on a cookbook checkout.
Write the bit-bang + DMI/flash layer directly in this component, using cnlohr
`esp32s2-cookbook/ch32v003programmer/main/{bitbang_rvswdio.h,ch32v003_swio.h}` and
`web3/lib/wchlink.js` as references (the latter is the validated DMI/flash oracle).

CH32V003 is **pure single-wire SWIO** (no SWCLK). Reused primitives:
`Send1BitSWIO`/`Send0BitSWIO`/`ReadBitSWIO`, `MCFWriteReg32`/`MCFReadReg32`,
`InitializeSWDSWIO`, `UnlockFlash`, `EraseFlash`, `Write64Block`/`WriteWord`,
`WaitForFlash`/`WaitForDoneOp`. All take `pinmaskD` → call with the gpio mask of
the target being flashed; program the 3 chips sequentially.

Porting work (the real risk):
1. **`PrecDelay` is Xtensa asm** (`addi`/`bbci`). Rewrite as a RISC-V busy loop
   (inline `asm` or `esp_cpu_get_cycle_count()` spin). C6 core = 160 MHz.
2. **Calibrate `t1coeff`** for 160 MHz (S2 was 240 MHz). Verify SWIO bit timing
   on a scope or by successful program+verify.
3. GPIO regs: `GPIO.out_w1ts/w1tc` (drive) and `GPIO.enable_w1ts/w1tc`
   (direction) exist on C6 — confirm field names in C6 `soc/gpio_struct.h`.
4. **Interrupt jitter**: WiFi/FreeRTOS ISRs corrupt cycle-counted timing. Wrap
   timed bit sequences in a critical section (`portENTER_CRITICAL` / disable
   interrupts) at per-word or per-64B-block granularity. Flashing briefly stalls
   WiFi for a few seconds — acceptable; do it in short windows.
5. SWIO needs a pull-up: prefer external 10k on each wire; else enable
   `R_GLITCH_HIGH` + 5 mA drive (internal-pull hack, less reliable).

Cross-check: the DMI register sequence (unlock keys, `CR_PAGE_*`, `DMCONTROL`
halt/reset, progbuf opcodes) is identical to the already-working
`web3/lib/wchlink.js` — use it as a reference oracle while bringing this up.

Reset/reflow without NRST: reset target via debug module `DMCONTROL` ndmreset
(no reset wire exists). Re-entering debug on a running chip relies on PD1 being a
USART-RX input + debug not disabled — **flagged risk, must be tested**.

**Programming-entry sequence (per target).** Runs only in Program mode (jumper
3.3 V–3.3 V, USB host present). The board powered up clean, so no "song and
dance":
1. Set the target gpio to open-drain SWIO with 3.3 V pull-up (it was UART TX).
2. SWIO connect + halt — same DMI sequence as `wchlink.js`
   `connect()/setupDM()/halt()` (DMCFGR/DMSHDWCFGR key, DMCONTROL haltreq). This
   halts the CH32 even though it had already started running its firmware (relies
   on the always-on debug tap — the reflash-running-chip test item).
3. Unlock flash → erase → program → verify (mirror `wchlink.js` `writeImage()`).
4. `DMCONTROL` ndmreset + resume (no NRST wire).
5. Restore the gpio to UART-TX idle-high. (Chip runs the new firmware; full effect
   seen after the next clean power-up in Operation mode.)

### CH32 provisioning model (decided: embedded + explicit-only)
- **One shared CH32 image** (all 3 expanders run identical firmware) is **baked
  into the ESP firmware** — embed `sw-v4/`'s `main.bin` via IDF
  `idf_component_register(... EMBED_FILES ...)` (raw, symbol `_binary_main_bin_start`;
  or a generated header, like
  web3's `EXPANDER_IMAGE`). Single source of truth, ships with ESP OTA.
- **Flash only on explicit command** — in Program mode, a **USB-serial** command
  from web4 ("flash expander N" / all 3) triggers it. **No auto-flash on boot**;
  the ESP never reprograms a CH32 unexpectedly. In Operation mode no USB ⇒ command
  can't arrive ⇒ no SWIO ever.
- web4 does **not** upload the image — it only triggers; the ESP flashes its
  embedded copy **unconditionally** on the button (A13: no version readback, no
  version gate — button = reflash). Transport = **WebSerial** (A11): the C6
  USB-Serial-JTAG enumerates as CDC; web4 opens it and sends a tiny command (e.g.
  `F0`/`F1`/`F2` to flash expander 0/1/2, or `FA` for all).
- The browser never drives bits — SWIO's ~1 µs timing can't survive WS latency;
  the ESP runs the whole programmer loop locally (Component B).

### Component C — `sw-panel-v4/` : ESP32-C6 app + hosted UI

**Two distinct web surfaces — don't conflate:**
- **Hosted UI** (this component): `sw-panel-v4/lib/` (`main.js` + `main.css`),
  bundled into `main/pages.h` and served by the ESP at `/`. This is the runtime
  control panel the device serves over WiFi.
- **Standalone tool** (Component D, `web4/`): the WebSerial/WebUSB programmer page.

**C.1 — firmware** (`sw-panel-v4/main/`). Fork of
`sw-panel-v3/main/sw-panel-v3-main.c`:
- Keep WiFi STA, HTTP/WS server, RGB-strip RMT path (`buf[0]==255`), config
  partition mmap.
- **Remove** all I2C (`i2c_master_*`, `rdwr_i2c`, dev handles).
- Add UART TX to gpio0/1/2. **A8 revised → single HW UART1, GPIO-matrix-switched.**
  Protocol is TX-only, one target per change, so one HW UART covers all 3: per send,
  route `U1TXD_OUT_IDX` to the target gpio via `esp_rom_gpio_connect_out_signal`,
  `uart_write_bytes` the 5-byte frame, park the other two pins as plain GPIO driven
  high (idle). HW FIFO shifts bits → immune to WiFi/FreeRTOS jitter (no critical
  section, unlike bit-bang). See "HW-UART1 TX refactor" below.
  - (superseded) bit-bang software UART on all 3 (`soft_uart.c`) — jitter-prone
    (frame drops on preempt, no re-send); becomes dead code after the refactor.
- Runtime: a non-RGB WS frame → translate to the 5-byte `0xAA|b0|b1|b2|csum`
  full-state frame and send on the addressed target's TX line. Frame[0]/[1] in v3
  selected the device; keep that selection to pick gpio0/1/2.
- **RF discipline (TX side).** Send a frame to a CH32 **only when its value
  changes** (diff against last-sent shadow); never stream/heartbeat. Hold each TX
  line idle-high (static) between frames. This keeps the data wires — the in-band
  noise source — quiet for the RF circuit (pairs with Component A's diff/sleep).
- Flash path (Program mode only): a **USB-serial** command from web4 carries
  **just {target}** (no image — CH32 binary embedded, see provisioning). Handler
  runs the entry sequence + `swio_programmer` against the embedded image on that
  gpio. Explicit-only; in Operation mode there's no USB so it can't fire.
- No boot-time role branch (A2): always boot operational (WiFi + UART TX) and also
  listen on USB-serial. SWIO happens only when a flash command arrives over USB —
  impossible in Operation mode (no USB host).
- Per-flash gpio interlock: switch the target gpio UART-TX → SWIO before flashing,
  back to UART-TX idle-high after.

**C.1a — HW-UART1 TX refactor (replaces `soft_uart.c`).** Sketch:

- Install once at boot: `uart_driver_install(UART_NUM_1, ...)` +
  `uart_param_config(UART_NUM_1, {baud=4800, 8N1})`. Do **not** call `uart_set_pin`
  (that pins TXD permanently); drive the matrix by hand instead.
- Park all 3 links idle-high as plain GPIO outputs at boot (as `suart_init` does now).
- Per frame to target `idx`:
  ```c
  static int cur = -1;               // gpio currently bound to U1TXD (-1 = none)
  static portMUX_TYPE link_mux = portMUX_INITIALIZER_UNLOCKED;
  void link_send(int idx, const uint8_t *f, size_t n) {
    portENTER_CRITICAL(&link_mux);   // serialize vs flash task + other sends
    int g = link_gpio[idx];
    if (cur != g) {
      if (cur >= 0) {                // park previous pin: plain GPIO, idle-high
        esp_rom_gpio_connect_out_signal(cur, SIG_GPIO_OUT_IDX, false, false);
        gpio_set_level(cur, 1);
      }
      esp_rom_gpio_connect_out_signal(g, U1TXD_OUT_IDX, false, false); // idle=high, no glitch
      cur = g;
    }
    portEXIT_CRITICAL(&link_mux);
    uart_write_bytes(UART_NUM_1, f, n);
    uart_wait_tx_done(UART_NUM_1, pdMS_TO_TICKS(50));
  }
  ```
- No per-bit critical section needed — HW shifter is jitter-proof (fixes F1).
- Interlock with flashing (fixes F3): before `swio_flash_image(g,…)`, if `cur==g`
  detach U1TXD (`SIG_GPIO_OUT_IDX`) so SWIO owns the pin; set `cur=-1`. `suart`/park
  restore after flash re-binds on the next `link_send`. Guard the whole flash under
  `link_mux` (or a higher-level flag) so a WS send can't drive the pin mid-flash.
- `U1TXD_OUT_IDX` / `SIG_GPIO_OUT_IDX` from `soc/uart_periph.h` / `soc/gpio_sig_map.h`.
- Delete `soft_uart.{c,h}` + its `SRCS` entry once `link_send` lands.

**C.2 — hosted UI** (`sw-panel-v4/lib/` + build → `main/pages.h`). Fork
`sw-panel-v3/lib/{main.js,main.css}`:
- Update the control surface for the v4 expander: **14 outputs** per
  panel, 3 panels; keep the RGB-strip control. (PD0/PC1/PC2 stay reserved inputs,
  A7 — b2 ignored; enable later for 17.) WS messages map to the firmware's
  full-state push (per-target 3-byte snapshot) + RGB path (`buf[0]==255`).
- Keep `document.XAPOH_VERSION` injection from `manifest.json`.
- Uses `onml` for DOM (as v3); no framework.

**C.3 — build flow: browserify → esbuild.** Rewrite `sw-panel-v3/page-to-h.js`
as `sw-panel-v4/page-to-h.js` using **esbuild** instead of browserify:
- `esbuild.build({entryPoints:['lib/main.js'], bundle:true, minify:true, write:false})`
  → take `outputFiles[0].text` as the script string (replaces the
  `browserify().bundle()` promise).
- Same downstream: wrap script+css in the HTML template, write `lib/index.html`,
  emit `main/pages.h` (`PAGE_index` byte array + `PAGE_index_length`, `DRAM_ATTR`).
- `package.json`: drop `browserify`, add `esbuild`; keep `onml`, `eslint`.
  (web3/web4 already build with esbuild via `web*/bin/build.js` — this aligns the
  hosted-UI build to the same toolchain.)

### Component D — `web4/` : standalone programmer tool (later)
The standalone WebSerial/WebUSB tool (NOT the device-hosted UI — that's C.2).
New folder `web4/` (do NOT modify `web3/`). Fork the `web3/` build
(esbuild → single `index.html`, `lib/` modules) as the starting point — web3 is
already on esbuild, so the toolchain carries over directly.

Current `web3/lib/wchlink.js` flashes CH32 via WebUSB WCH-LinkE. web4 instead
sends a **"flash expander N" trigger over WebSerial** (A11) — the ESP flashes its
*embedded* CH32 image via SWIO (Component B), so web4 carries no CH32 binary and
the WebUSB WCH-LinkE dependency is retired. Button = unconditional reflash, no
version readback (A13). ESP bridge flashing (esptool over WebSerial) unchanged.
web4 is the USB programmer tool **only** — no operational panel control (A12);
that lives in the WiFi hosted UI (C.2).

---

### Phasing
- **P0** Hardware: confirm gpio0/1/2 ↔ PD1 wiring; internal pull-up via
  `R_GLITCH_HIGH` + 5 mA drive (no external 10k); jumper switches CH32 VDD only;
  operation powered externally (no USB). (No level shifter — VIH@5V = 2.06 V; no
  mode strap — command-driven.) Confirm C6 GPIO struct field names.
- **P1** `sw-v4/` CH32 firmware: USART1 RX on PD1 + frame parser + 14-output GPIO
  map (PD0/PC1/PC2 as reserved inputs). Bench-test by driving the wire from a
  USB-serial adapter at 4800.
- **P2** `swio_programmer` component standalone: port bitbang, write RISC-V
  `PrecDelay`, calibrate `t1coeff`, flash one CH32 on gpio0, program+verify
  against a known image (oracle = wchlink.js result).
- **P3** `sw-panel-v4/` skeleton from v3 minus I2C; add 3× UART TX; end-to-end
  runtime control of all 3 panels over WiFi WS. Port the hosted UI (C.2): fork
  `lib/{main.js,main.css}`, rewrite `page-to-h.js` browserify→esbuild, regen
  `pages.h` for the 14-output surface. Run the UART-vs-SWIO non-conflict test.
- **P4** Embed `sw-v4` image into the ESP build (`EMBED_BINARIES`); WebSerial
  `F0/F1/F2/FA` command handler + entry sequence + per-flash gpio interlock; flash
  all 3 from a web4 button. Explicit-only, unconditional reflash, no auto-flash.
- **P5** `web4/` new UI: fork from web3, send the flash trigger to the ESP over USB
  (ESP flashes its embedded image via SWIO); retire WebUSB WCH-LinkE path. ESP
  bridge flashing (WebSerial/esptool) unchanged.

### Decisions (resolved)
- A1 Pull-up: **internal** `R_GLITCH_HIGH` + 5 mA drive (no external 10k).
- A2 Mode: **command-driven** — no strap/role-branch; USB flash command = only SWIO trigger.
- A3 Jumper: switches **only CH32 VDD** (3.3↔5 V).
- A4 Operation power: **external** source; no USB.
- A5 gpio0/1/2: **free** on the board (may relocate if an internal conflict shows up).
- A6 Baud: **4800**.
- A7 Outputs: **14 active now**; PD0/PC1/PC2 reserved as **inputs** (enable later).
- A8 3× TX: **REVISED → single HW UART1, GPIO-matrix-switched** (was bit-bang all 3).
  TX-only + one-target-per-change ⇒ one HW UART; FIFO removes bit-bang jitter.
- A9 All-zero-byte framing mitigation: **skip for now**.
- A10 SWIO programmer: **fresh in-repo port** (cookbook + wchlink.js as references).
- A11 web4↔ESP: **WebSerial** (C6 USB-Serial-JTAG CDC), tiny `F0/F1/F2/FA` command.
- A12 No overlap: web4 = USB programmer only; panel control = WiFi hosted UI.
- A13 No FW-version readback; web4 button = **unconditional reflash**.

### Open risks / to verify
1. RISC-V `PrecDelay` port + SWIO timing calibration at 160 MHz.
2. WiFi ISR jitter vs bit-banged SWIO — critical-section windowing.
3. Reflash a *running* CH32 with no NRST wire — in Program mode the chip powers up
   and runs before the ESP halts it; re-entry relies on the always-on debug tap.
   Clean power-on (jumper at 3.3/3.3) helps, but **must be bench-tested** (P2).
4. Level mismatch — RESOLVED: VIH@5V = 2.06 V < 3.3 V, no shifter. Only constraint:
   keep the link pull-up referenced to 3.3 V so the ESP pin never sees 5 V.
5. UART must never look like a SWIO debug transaction — mitigated by ~100× timing
   separation + "halt needs a full valid DMI write"; verify in P3 (see UART vs
   SWIO non-conflict). Keep baud low; optionally avoid all-zero payload bytes.
6. USART RX reliability at 1.5 MHz core (baud error + ISR latency) — 4800 = +0.16%, safe.
7. UART framing/resync robustness (no addressing, no ACK on a 1-way link).
