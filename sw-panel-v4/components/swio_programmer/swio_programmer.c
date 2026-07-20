#include "swio_programmer.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "soc/gpio_reg.h"
#include "esp_rom_sys.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_rom_gpio.h"      // esp_rom_gpio_connect_out_signal
#include "soc/gpio_sig_map.h"  // SIG_GPIO_OUT_IDX
#include "esp_cpu.h"           // esp_cpu_get_cycle_count
#include "rom/ets_sys.h"

static const char *TAG = "swio";

// R_GLITCH_HIGH fakes a pull-up when there's no external one by briefly driving
// the line high mid-read. With a real external pull-up (1k added on the bench)
// it causes bus contention against the chip's read-response low -> reads FF.
// Leave OFF whenever an external pull-up is fitted.
// #define R_GLITCH_HIGH
#define MAX_IN_TIMEOUT 256

// --- Debug Module register addresses (minichlink.h / wchlink.js) -------------
#define DMDATA0        0x04
#define DMDATA1        0x05
#define DMCONTROL      0x10
#define DMSTATUS       0x11
#define DMHARTINFO     0x12
#define DMABSTRACTCS   0x16
#define DMCOMMAND      0x17
#define DMABSTRACTAUTO 0x18
#define DMPROGBUF0     0x20
#define DMPROGBUF1     0x21
#define DMPROGBUF2     0x22
#define DMPROGBUF3     0x23
#define DMPROGBUF4     0x24
#define DMPROGBUF5     0x25
#define DMCFGR         0x7d
#define DMSHDWCFGR     0x7e

// --- CH32V003 flash peripheral ----------------------------------------------
#define FLASH_KEYR     0x40022004
#define FLASH_OBKEYR   0x40022008
#define FLASH_STATR    0x4002200c
#define FLASH_CTLR     0x40022010
#define FLASH_ADDR     0x40022014
#define FLASH_MODEKEYR 0x40022024
#define KEY1 0x45670123
#define KEY2 0xcdef89ab
#define CR_PAGE_PG  0x00010000
#define CR_PAGE_ER  0x00020000
#define CR_BUF_LOAD 0x00040000
#define CR_BUF_RST  0x00080000
#define CR_STRT     0x00000040
#define FLASH_BASE  0x08000000
#define SECTOR      64
#define CFGR_KEY    (0x5aa50000 | (1 << 10)) // allow output from slave

// --- bit-bang state ----------------------------------------------------------
typedef struct {
  uint32_t pinmask; // 1 << gpio (C6 has <32 GPIOs)
  int t1coeff;      // send bit timing
  int t1read;       // read-sample timing (chip drives its response at its own fixed
                    // fast rate; sampling must NOT scale with a slow send t1coeff)
  int prog_state;   // 0 none, 1 flash routine, 2 reg routine (program-buffer shape)
} swio_t;

static portMUX_TYPE swio_mux = portMUX_INITIALIZER_UNLOCKED;
#define DisableISR() portENTER_CRITICAL(&swio_mux)
#define EnableISR()  portEXIT_CRITICAL(&swio_mux)

// Fast single-store GPIO ops (REG_WRITE is one st on the C6).
#define D_LO(pm)  REG_WRITE(GPIO_OUT_W1TC_REG, (pm))
#define D_HI(pm)  REG_WRITE(GPIO_OUT_W1TS_REG, (pm))
#define D_DRV(pm) REG_WRITE(GPIO_ENABLE_W1TS_REG, (pm))
#define D_REL(pm) REG_WRITE(GPIO_ENABLE_W1TC_REG, (pm))
#define D_GET()   REG_READ(GPIO_IN_REG)

// RISC-V busy-loop delay (replaces the cookbook's Xtensa bbci loop). Loops while
// the counter is >= 0. Kept in IRAM and called only inside DisableISR windows.
static inline void IRAM_ATTR PrecDelay(int delay) {
  __asm__ volatile(
    "1: addi %0, %0, -1\n"
    "   bgez %0, 1b\n"
    : "+r"(delay));
}

static inline void IRAM_ATTR Send1Bit(uint32_t pm, int t1) {
  D_LO(pm); PrecDelay(t1); D_HI(pm); PrecDelay(t1);
}
static inline void IRAM_ATTR Send0Bit(uint32_t pm, int t1) {
  D_LO(pm); PrecDelay(t1 * 4); D_HI(pm); PrecDelay(t1);
}

// returns 0, 1, or 2 (timeout)
static inline int IRAM_ATTR ReadBit(uint32_t pm, int t1) {
  int ret;
  D_LO(pm);
  PrecDelay(t1);
  D_REL(pm);
  D_HI(pm);
#ifdef R_GLITCH_HIGH
  int halfwait = t1 / 2;
  PrecDelay(halfwait);
  D_DRV(pm);
  D_REL(pm);
  PrecDelay(halfwait);
#else
  PrecDelay(t1 * 2);
#endif
  ret = D_GET();
#ifdef R_GLITCH_HIGH
  if (!(ret & pm)) {
    PrecDelay(t1 * 2);
    D_DRV(pm);
    D_REL(pm);
  }
#endif
  for (int timeout = 0; timeout < MAX_IN_TIMEOUT; timeout++) {
    if (D_GET() & pm) {
      D_DRV(pm);
      PrecDelay(t1 / 2);
      return !!(ret & pm);
    }
  }
  D_DRV(pm); // force high and move on
  return 2;
}

// --- single-wire DMI register access (SWIO / opmode 0) -----------------------
static void IRAM_ATTR WriteReg32(swio_t *s, uint8_t cmd, uint32_t val) {
  uint32_t pm = s->pinmask;
  int t1 = s->t1coeff;
  D_HI(pm); D_DRV(pm);
  DisableISR();
  Send1Bit(pm, t1);
  for (uint32_t mask = 1u << 6; mask; mask >>= 1)
    (cmd & mask) ? Send1Bit(pm, t1) : Send0Bit(pm, t1);
  Send1Bit(pm, t1); // write marker
  for (uint32_t mask = 1u << 31; mask; mask >>= 1)
    (val & mask) ? Send1Bit(pm, t1) : Send0Bit(pm, t1);
  EnableISR();
  esp_rom_delay_us(8);
}

static int IRAM_ATTR ReadReg32(swio_t *s, uint8_t cmd, uint32_t *val) {
  uint32_t pm = s->pinmask;
  int t1 = s->t1coeff;
  D_HI(pm); D_DRV(pm);
  DisableISR();
  Send1Bit(pm, t1);
  for (uint32_t mask = 1u << 6; mask; mask >>= 1)
    (cmd & mask) ? Send1Bit(pm, t1) : Send0Bit(pm, t1);
  Send0Bit(pm, t1); // read marker
  int tr = s->t1read ? s->t1read : t1; // sample at fixed fast rate, not slow send t1
  uint32_t rval = 0;
  for (int i = 0; i < 32; i++) {
    rval <<= 1;
    int r = ReadBit(pm, tr);
    if (r == 1) rval |= 1;
    if (r == 2) { EnableISR(); return -1; }
  }
  *val = rval;
  EnableISR();
  esp_rom_delay_us(8);
  return 0;
}

// --- Debug-Module helpers (mirror wchlink.js) --------------------------------
static int waitForDoneOp(swio_t *s) {
  uint32_t cs = 0;
  int t = 100;
  do { if (ReadReg32(s, DMABSTRACTCS, &cs)) return -1; } while ((cs & (1 << 12)) && t-- > 0);
  if (((cs >> 8) & 7) || (cs & (1 << 12))) {
    WriteReg32(s, DMABSTRACTCS, 0x00000700); // clear errors
    return -1;
  }
  return 0;
}

// Read a 32-bit target word. Clobbers x8 / PROGBUF0 -> invalidates write routine.
static int readWord(swio_t *s, uint32_t addr, uint32_t *out) {
  WriteReg32(s, DMABSTRACTAUTO, 0);
  WriteReg32(s, DMPROGBUF0, 0x90024000); // c.lw x8,0(x8) ; c.ebreak
  WriteReg32(s, DMDATA0, addr);
  WriteReg32(s, DMCOMMAND, 0x00271008);  // DATA0 -> x8, execute
  if (waitForDoneOp(s)) return -1;
  WriteReg32(s, DMCOMMAND, 0x00221008);  // x8 -> DATA0
  s->prog_state = 0;
  return ReadReg32(s, DMDATA0, out);
}

static int waitForFlash(swio_t *s) {
  uint32_t rw = 0;
  int t = 1000;
  do { if (readWord(s, FLASH_STATR, &rw)) return -1; } while ((rw & 3) && t-- > 0);
  if (rw & 0x10) return -1; // write-protect
  if (t <= 0) return -1;
  return 0;
}

// Load x10=&DATA0, x11=&DATA1, x12=&FLASH_STATR, x13=PAGE_PG|BUF_LOAD.
static void updateProgbufRegs(swio_t *s) {
  uint32_t rr = 0;
  ReadReg32(s, DMHARTINFO, &rr);
  uint32_t data0 = 0xe0000000 | (rr & 0x7ff);
  WriteReg32(s, DMABSTRACTAUTO, 0);
  WriteReg32(s, DMDATA0, data0);                 WriteReg32(s, DMCOMMAND, 0x0023100a);
  WriteReg32(s, DMDATA0, data0 + 4);             WriteReg32(s, DMCOMMAND, 0x0023100b);
  WriteReg32(s, DMDATA0, FLASH_STATR);           WriteReg32(s, DMCOMMAND, 0x0023100c);
  WriteReg32(s, DMDATA0, CR_PAGE_PG | CR_BUF_LOAD); WriteReg32(s, DMCOMMAND, 0x0023100d);
}

static void ensureWriteProg(swio_t *s, int flash) {
  int want = flash ? 1 : 2;
  if (s->prog_state == want) return;
  WriteReg32(s, DMABSTRACTAUTO, 0);
  updateProgbufRegs(s);
  WriteReg32(s, DMPROGBUF0, 0x41844100); // c.lw x8,0(x10) ; c.lw x9,0(x11)
  WriteReg32(s, DMPROGBUF1, 0x0491c080); // c.sw x8,0(x9)  ; c.addi x9,4
  if (flash) {
    WriteReg32(s, DMPROGBUF2, 0x0001c184); // c.sw x9,0(x11) ; c.nop
    WriteReg32(s, DMPROGBUF3, 0x4200c254); // c.sw x13,4(x12); c.lw x8,0(x12)
    WriteReg32(s, DMPROGBUF4, 0xfc758805); // c.andi x8,1    ; c.bnez x8,-4
    WriteReg32(s, DMPROGBUF5, 0x90029002); // c.ebreak       ; c.ebreak
  } else {
    WriteReg32(s, DMPROGBUF2, 0x9002c184); // c.sw x9,0(x11) ; c.ebreak
  }
  s->prog_state = want;
}

static int isFlash(uint32_t addr) { return (addr & 0xe0000000) == 0; }

static int writeWord(swio_t *s, uint32_t addr, uint32_t val) {
  int flash = isFlash(addr);
  ensureWriteProg(s, flash);
  WriteReg32(s, DMDATA1, addr);
  WriteReg32(s, DMDATA0, val);
  WriteReg32(s, DMCOMMAND, 0x00240000); // execute program buffer
  if (flash) return waitForDoneOp(s);
  return 0;
}

static void halt(swio_t *s) {
  WriteReg32(s, DMSHDWCFGR, CFGR_KEY);
  WriteReg32(s, DMCFGR, CFGR_KEY);
  WriteReg32(s, DMCONTROL, 0x80000001);
  WriteReg32(s, DMCONTROL, 0x80000003); // reset
  WriteReg32(s, DMCONTROL, 0x80000001); // re-halt
  WriteReg32(s, DMCONTROL, 0x80000001);
  esp_rom_delay_us(10000);
  s->prog_state = 0;
}

// SWIO connect — mirrors the validated web3/lib/wchlink.js setupDM(). DMCFGR
// (0x7d) is write-only: it does NOT return the key on read, so presence must be
// detected by reading DMSTATUS (0x11), pass if not 0x0 / 0xffffffff (both = no
// chip / bus idle). DMCONTROL must carry haltreq|dmactive (0x80000001).
static int connect(swio_t *s) {
  for (int tries = 3; tries > 0; tries--) {
    esp_rom_delay_us(16000); // minichlink DefaultSetupInterface settle delay
    WriteReg32(s, DMSHDWCFGR, CFGR_KEY);
    WriteReg32(s, DMCFGR, CFGR_KEY);
    WriteReg32(s, DMSHDWCFGR, CFGR_KEY);
    WriteReg32(s, DMCFGR, CFGR_KEY);
    WriteReg32(s, DMCONTROL, 0x80000001);
    WriteReg32(s, DMCONTROL, 0x80000001);
    WriteReg32(s, DMCONTROL, 0x80000001);
    uint32_t st = 0;
    if (ReadReg32(s, DMSTATUS, &st) == 0 && st != 0x00000000 && st != 0xffffffff) {
      return 0;
    }
    ESP_LOGE(TAG, "no RVSWIO chip (DMSTATUS=0x%08x)", (unsigned)st);
  }
  return -1;
}

// Sweep t1coeff and log which values get the chip to answer. On the first
// working value the chip is connected and left ready. Remove/replace with a
// fixed SWIO_T1COEFF once the good value is known (calibration aid only).
// Measure the real duration of PrecDelay for each candidate and log it in ns.
// Removes the scope: confirms the sweep actually spans the chip's SWIO window
// (Send1Bit low = PrecDelay(t1); Send0Bit low = PrecDelay(t1*4)).
static void report_timing(void) {
  static const int cand[] = { 20, 40, 80, 160, 640 };
  uint32_t f = esp_rom_get_cpu_ticks_per_us(); // ticks per us
  for (unsigned i = 0; i < sizeof(cand) / sizeof(cand[0]); i++) {
    portMUX_TYPE m = portMUX_INITIALIZER_UNLOCKED;
    portENTER_CRITICAL(&m);
    uint32_t t0 = esp_cpu_get_cycle_count();
    PrecDelay(cand[i]);
    uint32_t dt = esp_cpu_get_cycle_count() - t0;
    portEXIT_CRITICAL(&m);
    ESP_LOGW(TAG, "timing: t1coeff=%d -> Send1 low=%u ns (Send0 low ~%u ns)",
             cand[i], (unsigned)(dt * 1000 / f), (unsigned)(dt * 4 * 1000 / f));
  }
}

static int calibrate(swio_t *s) {
  report_timing();

  // First try the compile-time default SWIO_T1COEFF, so a known-good value can
  // be set without sweeping if desired.
  int tried_default = 0;
  if (SWIO_T1COEFF > 0) {
    s->t1coeff = SWIO_T1COEFF;
    s->t1read = 40;  // fixed-fast read (~500 ns) independent of send speed
    s->prog_state = 0;
    if (connect(s) == 0) {
      ESP_LOGW(TAG, "CALIBRATED (fixed): send t1coeff=%d, read t1=%d works", s->t1coeff, s->t1read);
      return 0;
    }
    ESP_LOGW(TAG, "fixed t1coeff=%d (read t1=%d): no answer, sweeping", s->t1coeff, s->t1read);
    tried_default = 1;
  }

  static const int cand[] = { 12, 16, 20, 28, 36, 48, 64, 80, 120, 160, 320, 400, 640, 800, 1000, 1200, 1600, 2000, 2400, 3200 };
  for (unsigned i = 0; i < sizeof(cand) / sizeof(cand[0]); i++) {
    if (tried_default && cand[i] == SWIO_T1COEFF) continue; // already tried
    s->t1coeff = cand[i];      // SEND timing (slow, to activate the DM like LinkE)
    s->t1read = 40;            // READ sampling fixed-fast (~500 ns), independent of send
    s->prog_state = 0;
    if (connect(s) == 0) {
      ESP_LOGW(TAG, "CALIBRATED: send t1coeff=%d, read t1=%d works", s->t1coeff, s->t1read);
      return 0;
    }
    ESP_LOGW(TAG, "send t1coeff=%d (read t1=%d): no answer", s->t1coeff, s->t1read);
  }
  ESP_LOGE(TAG, "calibrate: no send/read combo in sweep worked");
  return -1;
}

static int unlockFlash(swio_t *s) {
  uint32_t ctlr = 0;
  readWord(s, FLASH_CTLR, &ctlr);
  writeWord(s, FLASH_KEYR, KEY1);
  writeWord(s, FLASH_KEYR, KEY2);
  writeWord(s, FLASH_OBKEYR, KEY1);
  writeWord(s, FLASH_OBKEYR, KEY2);
  writeWord(s, FLASH_MODEKEYR, KEY1);
  writeWord(s, FLASH_MODEKEYR, KEY2);
  readWord(s, FLASH_CTLR, &ctlr);
  if (ctlr & 0x80) { ESP_LOGE(TAG, "flash unlock failed (CTLR=0x%08x)", (unsigned)ctlr); return -1; }
  return 0;
}

static int eraseSector(swio_t *s, uint32_t base) {
  writeWord(s, FLASH_CTLR, CR_PAGE_ER);
  writeWord(s, FLASH_ADDR, base);
  writeWord(s, FLASH_CTLR, CR_STRT | CR_PAGE_ER);
  return waitForFlash(s);
}

// --- SWIO line-level bring-up -----------------------------------------------
// Long low pulse + short debug-module reset, then a LinkE-style short-pulse
// handshake before any DMI traffic. This closely matches the observed
// WCH-LinkE waveform: initial ~1.2us pulses, a gap, then config writes.
static void swio_line_reset(swio_t *s) {
  uint32_t pm = s->pinmask;

  // Idle high for a moment via push-pull to fully charge the line.
  D_DRV(pm);
  D_HI(pm);
  esp_rom_delay_us(5000); // ~5 ms

  // Long low pulse to force the target into SWIO/program mode.
  D_LO(pm);
  esp_rom_delay_us(20000); // ~20 ms

  // Release back to idle high; line then held by external pull-up.
  D_HI(pm);
  D_REL(pm);
  esp_rom_delay_us(100);

  // Short low pulse to reset the debug module itself, PicoSWIO-style.
  D_DRV(pm);
  D_LO(pm);
  esp_rom_delay_us(8); // 8–30 us works; keep it short to avoid re-entering ISP
  D_HI(pm);
  D_REL(pm);
  esp_rom_delay_us(10);
}

// 32 short "1" bits at the selected t1coeff, then a ~2.2 ms gap. This emulates
// the initial WCH-LinkE handshake pulse train seen on the scope.
static void swio_handshake(swio_t *s) {
  uint32_t pm = s->pinmask;
  int t1 = s->t1coeff;

  D_HI(pm);
  D_DRV(pm);
  DisableISR();
  for (int i = 0; i < 32; i++) {
    Send1Bit(pm, t1);
  }
  EnableISR();

  esp_rom_delay_us(2200); // ~2.2 ms idle gap
}

static void reboot(swio_t *s) {
  WriteReg32(s, DMCONTROL, 0x80000001);
  WriteReg32(s, DMCONTROL, 0x80000001);
  WriteReg32(s, DMCONTROL, 0x80000003); // reset
  WriteReg32(s, DMCONTROL, 0x40000001); // resumereq
}

static uint32_t rdword(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

// Bring-up aid: toggle the pin ~50 kHz for ~1 s so a scope can confirm the ESP
// actually drives the pad (rules out GPIO routing / wrong pin). Same pad config
// as the flasher. Call from the F0 handler instead of swio_flash_image to test.
void swio_pin_selftest(int gpio) {
  gpio_reset_pin(gpio);
  gpio_set_pull_mode(gpio, GPIO_PULLUP_ONLY);
  gpio_set_direction(gpio, GPIO_MODE_INPUT_OUTPUT);
  gpio_set_drive_capability(gpio, GPIO_DRIVE_CAP_2); // external pull-up: normal drive
  esp_rom_gpio_connect_out_signal(gpio, SIG_GPIO_OUT_IDX, false, false);
  uint32_t pm = 1u << gpio;
  // Phase A: driver-level slow blink (5 Hz, 2 s). Uses gpio_set_level only —
  // no REG macros, no GPIO matrix. If a scope/LED shows NOTHING here, the pin
  // itself is the problem (reserved/wrong/fought), not the SWIO bit-bang.
  gpio_set_direction(gpio, GPIO_MODE_OUTPUT);
  ESP_LOGW(TAG, "selftest A: gpio%d 5Hz driver blink, 2s", gpio);
  for (int i = 0; i < 10; i++) {
    gpio_set_level(gpio, i & 1);
    esp_rom_delay_us(100000);
  }
  // Phase B: REG-macro 50 kHz square (what SWIO actually uses).
  gpio_set_direction(gpio, GPIO_MODE_INPUT_OUTPUT);
  ESP_LOGW(TAG, "selftest B: gpio%d 50kHz REG toggle, 1s", gpio);
  for (int i = 0; i < 25000; i++) {
    D_DRV(pm); D_LO(pm);
    esp_rom_delay_us(10);
    D_HI(pm);
    esp_rom_delay_us(10);
  }
  // Phase C: READ monitor. Pin = input (external 1k holds it high). Log the
  // level 10x over 5 s. Short gpio0 to GND by hand during this window: the log
  // MUST flip to 0. If it stays 1, the ESP read path is broken (not the chip).
  gpio_set_direction(gpio, GPIO_MODE_INPUT);
  ESP_LOGW(TAG, "selftest C: read monitor 5s -- short gpio%d to GND now", gpio);
  for (int i = 0; i < 10; i++) {
    ESP_LOGW(TAG, "selftest C: gpio%d reads %d", gpio, !!(D_GET() & pm));
    esp_rom_delay_us(500000);
  }
  ESP_LOGW(TAG, "selftest: done");
}

// --- public ------------------------------------------------------------------
esp_err_t swio_flash_image(int gpio, const uint8_t *bin, size_t len) {
  if (gpio < 0 || gpio > 30 || !bin || len == 0) return ESP_ERR_INVALID_ARG;

  // Pad config: GPIO function, push-pull capable (we tri-state via the ENABLE
  // register in the bit-bang), internal pull-up, low (~5 mA) drive for R_GLITCH.
  gpio_reset_pin(gpio);
  gpio_set_pull_mode(gpio, GPIO_PULLUP_ONLY);
  gpio_set_direction(gpio, GPIO_MODE_INPUT_OUTPUT);
  gpio_set_drive_capability(gpio, GPIO_DRIVE_CAP_2); // external pull-up: normal drive
  // Detach any peripheral (UART U1TXD) from the pad: the bit-bang drives via the
  // GPIO_OUT register, which only reaches the pad when out-sel = simple GPIO.
  esp_rom_gpio_connect_out_signal(gpio, SIG_GPIO_OUT_IDX, false, false);
  gpio_set_level(gpio, 1);

  swio_t st = { .pinmask = (1u << gpio), .t1coeff = SWIO_T1COEFF, .prog_state = 0 };

  // Line-level bring-up and initial handshake before any DMI traffic. This
  // closely matches the WCH-LinkE sequence (long low -> short pulses -> gap).
  swio_line_reset(&st);
  swio_handshake(&st);

  if (calibrate(&st)) return ESP_FAIL;
  halt(&st);
  if (unlockFlash(&st)) return ESP_FAIL;

  size_t total = (len + SECTOR - 1) / SECTOR;
  for (size_t sct = 0; sct < total; sct++) {
    uint32_t base = FLASH_BASE + sct * SECTOR;
    if (eraseSector(&st, base)) { ESP_LOGE(TAG, "erase fail @0x%08x", (unsigned)base); return ESP_FAIL; }
    writeWord(&st, FLASH_CTLR, CR_PAGE_PG);
    writeWord(&st, FLASH_CTLR, CR_BUF_RST | CR_PAGE_PG);
    if (waitForFlash(&st)) return ESP_FAIL;
    for (int j = 0; j < SECTOR / 4; j++) {
      size_t o = sct * SECTOR + j * 4;
      uint8_t buf[4] = {0xff, 0xff, 0xff, 0xff};
      for (int k = 0; k < 4 && o + k < len; k++) buf[k] = bin[o + k];
      writeWord(&st, base + j * 4, rdword(buf));
    }
    writeWord(&st, FLASH_ADDR, base);
    writeWord(&st, FLASH_CTLR, CR_PAGE_PG | CR_STRT); // commit page
    if (waitForFlash(&st)) return ESP_FAIL;
  }

  // verify
  for (size_t o = 0; o < len; o += 4) {
    uint8_t buf[4] = {0xff, 0xff, 0xff, 0xff};
    for (int k = 0; k < 4 && o + k < len; k++) buf[k] = bin[o + k];
    uint32_t expect = rdword(buf), got = 0;
    if (readWord(&st, FLASH_BASE + o, &got) || got != expect) {
      ESP_LOGE(TAG, "verify fail @0x%08x: 0x%08x != 0x%08x", (unsigned)(FLASH_BASE + o), (unsigned)got, (unsigned)expect);
      return ESP_FAIL;
    }
  }

  reboot(&st);
  ESP_LOGI(TAG, "flashed %u bytes on gpio%d", (unsigned)len, gpio);
  return ESP_OK;
}
