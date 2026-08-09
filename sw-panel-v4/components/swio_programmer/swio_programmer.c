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
#include "esp_timer.h"         // esp_timer_get_time (swio_listen deadline)
#include "rom/ets_sys.h"

static const char *TAG = "swio";

// R_GLITCH_HIGH: the cookbook's macro for glitch-based reads. Our ReadBit uses a
// custom inline glitch (D_HI then D_REL) instead of this macro path, so this
// #define is left OFF — the actual glitch logic is hardcoded in ReadBit below.
// No external pull-up resistor: reads work via the inline glitch + the CH32's
// internal pull-up. This is the bench-validated configuration (all 3 expanders
// flash successfully with no external resistor).
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

// SDI bit encoding — matches the WCH-LinkE observed waveform:
//   WRITE bits use STRONG-1 (driven high): the ESP drives both low and high.
//     1 = short strong-0, strong-1
//     0 = long  strong-0, strong-1
//   READ bits use a glitch+release (see ReadBit) so the chip can drive back.
// The driver stays ENABLED for the whole write transaction (from the preamble);
// send bits just toggle OUT, never touch ENABLE.
// PrecDelay values are hardcoded — tuned on the bench for the no-external-resistor
// configuration (internal pull-up only + glitch-based reads).
static inline void IRAM_ATTR Send1Bit(uint32_t pm) {
  D_LO(pm); PrecDelay(1); D_HI(pm); // strong-0 ~250ns, strong-1 ~250ns
}
static inline void IRAM_ATTR Send0Bit(uint32_t pm) {
  D_LO(pm); PrecDelay(40); D_HI(pm); // strong-0 ~750ns, strong-1 ~250ns
}

// READ bit (glitch-based, no external pull-up): drive a short strong-0 clock,
// then a brief D_HI glitch to charge the line fast (the internal ~45k pull-up
// alone is too slow), then RELEASE so the chip can drive its response low.
// Sample immediately after the release, then re-drive high for the next bit.
// This is the bench-validated read path — all 3 expanders flash with no ext R.
// Returns the sampled bit (0 or 1).
static inline int IRAM_ATTR ReadBit(uint32_t pm) {
  int ret;
  D_LO(pm);
  PrecDelay(1); // ~250ns strong-0
  D_HI(pm);     // ~250ns glitch: charge the line to 3.3V fast
  D_REL(pm);    // release — chip drives its response here
  ret = D_GET() & pm; // sample immediately after the release
  PrecDelay(1); // ~250ns wait for the chip to finish driving (if it drives)
  D_DRV(pm);    // re-drive high for the next bit
  return !!(ret & pm);
}

// --- single-wire DMI register access (SWIO / opmode 0) -----------------------
// Write bits toggle OUT only (driver stays enabled, strong-1 highs). ReadBit
// releases (D_REL) for the chip's response then re-drives high (D_DRV). After
// the last bit the line is idle-high.
static void IRAM_ATTR WriteReg32(swio_t *s, uint8_t cmd, uint32_t val) {
  uint32_t pm = s->pinmask;
  D_HI(pm); D_DRV(pm);
  DisableISR();
  Send1Bit(pm);
  for (uint32_t mask = 1u << 6; mask; mask >>= 1)
    (cmd & mask) ? Send1Bit(pm) : Send0Bit(pm);
  Send1Bit(pm); // write marker
  for (uint32_t mask = 1u << 31; mask; mask >>= 1)
    (val & mask) ? Send1Bit(pm) : Send0Bit(pm);
  D_HI(pm); D_DRV(pm); // idle-high for the gap
  EnableISR();
  esp_rom_delay_us(8); // inter-transaction gap (cookbook: "sometimes 2 is too short")
}

static int IRAM_ATTR ReadReg32(swio_t *s, uint8_t cmd, uint32_t *val) {
  uint32_t pm = s->pinmask;
  D_HI(pm); D_DRV(pm);
  DisableISR();
  Send1Bit(pm);
  for (uint32_t mask = 1u << 6; mask; mask >>= 1)
    (cmd & mask) ? Send1Bit(pm) : Send0Bit(pm);
  Send0Bit(pm); // read marker
  uint32_t rval = 0;
  for (int i = 0; i < 32; i++) {
    rval <<= 1;
    int r = ReadBit(pm);
    if (r == 1) rval |= 1;
    if (r == 2) { EnableISR(); return -1; }
  }
  *val = rval;
  D_HI(pm); D_DRV(pm); // idle-high for the gap
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
    ESP_LOGE(TAG, "waitForDoneOp: abstract cmd fault (DMABSTRACTCS=0x%08x)", (unsigned)cs);
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
  if (rw & 0x10) { ESP_LOGE(TAG, "waitForFlash: WRPROT (STATR=0x%08x)", (unsigned)rw); return -1; }
  if (t <= 0) { ESP_LOGE(TAG, "waitForFlash: timeout (STATR=0x%08x)", (unsigned)rw); return -1; }
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

// SWIO connect — bring up the debug module. Writes CFGR_KEY to DMSHDWCFGR/DMCFGR
// (allow slave output), sets DMCONTROL dmactive (NO haltreq during probe), then
// reads DMCFGR back and checks the 0x5aa5 key echoes. The first read often
// returns a bit-shifted echo (0xad52...) — the 3-retry loop handles that; the
// chip's SDI needs one "wake" transaction before it responds cleanly.
static int connect(swio_t *s) {
  for (int tries = 3; tries > 0; tries--) {
    WriteReg32(s, DMSHDWCFGR, CFGR_KEY);
    WriteReg32(s, DMCFGR, CFGR_KEY);
    WriteReg32(s, DMSHDWCFGR, CFGR_KEY);
    WriteReg32(s, DMCFGR, CFGR_KEY);
    WriteReg32(s, DMCONTROL, 0x00000001); // dmactive only — NO haltreq during probe
    WriteReg32(s, DMCONTROL, 0x00000001);
    uint32_t cfgr = 0xDEADBEEF;
    int rc = ReadReg32(s, DMCFGR, &cfgr);
    if (rc == 0 && (cfgr & 0xffff0000) == 0x5aa50000) {
      return 0;
    }
    uint32_t st = 0xDEADBEEF;
    int rc2 = ReadReg32(s, DMSTATUS, &st);
    ESP_LOGE(TAG, "no chip: rc=%d cfgr=0x%08x rc2=%d st=0x%08x (%s)",
             rc, (unsigned)cfgr, rc2, (unsigned)st,
             (rc < 0) ? "line stuck LOW (read timeout)" :
             (cfgr == 0xffffffff) ? "line stuck HIGH (no chip drive)" : "bad echo");
  }
  return -1;
}

// Forward decl: connect_retry() calls swio_line_reset, defined further down.
// IRAM_ATTR is on the definition only — the macro uses __COUNTER__ for section
// names, so adding it here too causes a conflicting-section error.
static void swio_line_reset(swio_t *s);

// Connect to the chip: send the SDI wakeup preamble (swio_line_reset) then try
// connect(). The chip's SDI often needs a few wakeup attempts before it locks
// (the first read returns a bit-shifted echo 0xad52...; a later retry returns
// the clean 0x5aa5... echo). This is an honest retry loop — there is no timing
// sweep because the bit functions use hardcoded PrecDelay values (tuned on the
// bench). Bench-validated: all 3 expanders connect within ~5 retries.
static int connect_retry(swio_t *s) {
  s->prog_state = 0;
  for (int tries = 10; tries > 0; tries--) {
    swio_line_reset(s);
    if (connect(s) == 0) {
      ESP_LOGW(TAG, "CONNECTED after %d tries", 10 - tries + 1);
      return 0;
    }
  }
  ESP_LOGE(TAG, "connect: chip never responded after 10 tries");
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
  int r;
  r = writeWord(s, FLASH_CTLR, CR_PAGE_ER);
  if (r) { ESP_LOGE(TAG, "erase: writeWord CTLR=CR_PAGE_ER failed", 0); return -1; }
  r = writeWord(s, FLASH_ADDR, base);
  if (r) { ESP_LOGE(TAG, "erase: writeWord ADDR=0x%08x failed", (unsigned)base); return -1; }
  r = writeWord(s, FLASH_CTLR, CR_STRT | CR_PAGE_ER);
  if (r) { ESP_LOGE(TAG, "erase: writeWord CTLR=CR_STRT|CR_PAGE_ER failed", 0); return -1; }
  return waitForFlash(s);
}

static void reboot(swio_t *s) {
  WriteReg32(s, DMCONTROL, 0x80000001);
  WriteReg32(s, DMCONTROL, 0x80000001);
  WriteReg32(s, DMCONTROL, 0x80000003); // reset
  WriteReg32(s, DMCONTROL, 0x40000001); // resumereq
}

// SDI Hardware Synchronization and Wakeup Preamble — the 32 strong-0 / weak-1
// pulse train the WCH-LinkE sends before any DMI traffic. Because the CH32V003
// 1-wire debug interface has no separate clock line, its SDI hardware uses this
// sequence for three functions before handling register commands:
//   1. Auto-baud / time-quanta lock: the chip measures the ~800ns low + ~800ns
//      high pulse width to calibrate its internal bit-sampling clock to the
//      programmer's speed.
//   2. Peripheral override (PD1 un-remapping): if user firmware remapped PD1 to
//      another function (e.g. USART1_RX), the debug override logic detects these
//      pulses and forcibly re-routes PD1 back to the SDI debug peripheral.
//   3. Core wakeup: if the CPU is in __WFI / sleep, the sequence wakes the core
//      clock and debug domain before command frames arrive.
// This is required here because the target runs sw-v4 (remaps PD1 to USART1 RX
// + sleeps in __WFI). Without it the chip never responds to DMI traffic.
// Waveform matches the LinkE: 32 pulses (~1.6us each) + ~950us idle gap. The
// high half is a pure release (D_REL) — the chip's internal pull-up raises the
// line; no driven high.
static void IRAM_ATTR swio_line_reset(swio_t *s) {
  uint32_t pm = s->pinmask;
  D_HI(pm); D_DRV(pm);
  esp_rom_delay_us(50);

  // 32 strong-0 / weak-1 pulses. D_LO arms OUT=0 once (outside the loop); each
  // iteration enables the driver (strong-0), delays, then releases (weak-1) and
  // delays. PrecDelay(27) compensates for store + loop overhead so each half is
  // ~800ns on the wire, matching the LinkE.
  D_LO(pm);
  for (int i = 0; i < 32; i++) {
    D_DRV(pm);      // strong-0
    PrecDelay(27);
    D_REL(pm);      // weak-1: release (CH32 internal pull-up raises line)
    PrecDelay(27);
  }

  // ~950us gap: line held idle-high (driver enabled, no external pull-up).
  esp_rom_delay_us(950);
}

static uint32_t rdword(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

// Bring-up aid: toggle the pin ~50 kHz for ~1 s so a scope can confirm the ESP
// actually drives the pad (rules out GPIO routing / wrong pin). Same pad config
// as the flasher. Call from the F0 handler instead of swio_flash_image to test.
void swio_pin_selftest(int gpio) {
  gpio_reset_pin(gpio);
  gpio_set_pull_mode(gpio, GPIO_PULLUP_ONLY); // match swio_pad_setup
  gpio_set_direction(gpio, GPIO_MODE_INPUT_OUTPUT);
  gpio_set_drive_capability(gpio, GPIO_DRIVE_CAP_2); // normal drive (no R_GLITCH_HIGH)
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
  // Phase C: READ monitor. Pin = input (internal pull-up holds it high). Log
  // the level 10x over 5 s. Short the gpio to GND by hand during this window:
  // the log MUST flip to 0. If it stays 1, the ESP read path is broken.
  gpio_set_direction(gpio, GPIO_MODE_INPUT);
  ESP_LOGW(TAG, "selftest C: read monitor 5s -- short gpio%d to GND now", gpio);
  for (int i = 0; i < 10; i++) {
    ESP_LOGW(TAG, "selftest C: gpio%d reads %d", gpio, !!(D_GET() & pm));
    esp_rom_delay_us(500000);
  }

  // Phase D: SWIO-line traffic monitor. Send a real DMI write, then sample the
  // pin ~600 times over the response window. If the chip ever pulls low, at
  // least one sample reads 0 (chip is alive). If every sample is 1, the chip
  // never drives -> dead wire / wrong pin / not powered / debug disabled.
  gpio_set_direction(gpio, GPIO_MODE_INPUT_OUTPUT);
  swio_t st = { .pinmask = pm, .prog_state = 0 };
  ESP_LOGW(TAG, "selftest D: SWIO traffic, sampling line after DMI write");
  WriteReg32(&st, DMSHDWCFGR, CFGR_KEY);
  int lows = 0, samples = 0;
  for (int i = 0; i < 600; i++) {
    if (!(D_GET() & pm)) lows++;
    samples++;
    PrecDelay(4); // ~50 ns per sample -> ~30 us window covers a read response
  }
  ESP_LOGW(TAG, "selftest D: %d/%d samples were LOW (chip drove the line)", lows, samples);

  ESP_LOGW(TAG, "selftest: done");
}

// --- public ------------------------------------------------------------------
// Pad setup shared by swio_flash_image and swio_listen.
static void swio_pad_setup(int gpio) {
  gpio_reset_pin(gpio);
  // Internal pull-up enabled — with no external resistor, this is the ONLY
  // pull-up. The CH32V003 also has its own internal pull-up on PD1; both pull
  // the line high during read releases and idle. Matches the LinkE which relies
  // on the chip's internal pull-up.
  gpio_set_pull_mode(gpio, GPIO_PULLUP_ONLY);
  gpio_set_direction(gpio, GPIO_MODE_INPUT_OUTPUT);
  gpio_set_drive_capability(gpio, GPIO_DRIVE_CAP_2); // normal drive (no R_GLITCH_HIGH)
  esp_rom_gpio_connect_out_signal(gpio, SIG_GPIO_OUT_IDX, false, false);
  gpio_set_level(gpio, 1);
}

esp_err_t swio_listen(int gpio, int seconds) {
  if (gpio < 0 || gpio > 30) return ESP_ERR_INVALID_ARG;
  swio_pad_setup(gpio);
  swio_t s = { .pinmask = (1u << gpio), .prog_state = 0 };
  ESP_LOGW(TAG, "listen: hammering line-reset+connect on gpio%d for %d s — power-cycle the CH32 NOW",
           gpio, seconds);
  int64_t end = esp_timer_get_time() + (int64_t)seconds * 1000000;
  int tries = 0;
  while (esp_timer_get_time() < end) {
    tries++;
    swio_line_reset(&s);   // re-arm the SDI state machine each attempt
    if (connect(&s) == 0) {
      uint32_t st = 0;
      ReadReg32(&s, DMSTATUS, &st);
      ESP_LOGW(TAG, "listen: CHIP ANSWERED at try %d! DMSTATUS=0x%08x", tries, (unsigned)st);
      return ESP_OK; // got it — leave connected for the caller
    }
    vTaskDelay(1); // yield so we don't starve the flash_cmd task / watchdog
  }
  ESP_LOGE(TAG, "listen: chip never answered in %d tries over %d s", tries, seconds);
  return ESP_FAIL;
}

esp_err_t swio_flash_image(int gpio, const uint8_t *bin, size_t len) {
  if (gpio < 0 || gpio > 30 || !bin || len == 0) return ESP_ERR_INVALID_ARG;

  swio_pad_setup(gpio);

  // Idle-line check: release the ESP, let the internal pull-up settle, read.
  // MUST read 1 (internal pull-up + chip's pull-up hold the line high). Aborts
  // if LOW (short to GND / chip holding PD1). Non-intrusive — no glitch before
  // the handshake.
  uint32_t pm = (1u << gpio);
  D_REL(pm); D_HI(pm);
  esp_rom_delay_us(1000);
  int idle_high = !!(D_GET() & pm);
  ESP_LOGW(TAG, "line check: idle=%d (expect 1; 0 = line held LOW)", idle_high);
  if (!idle_high) {
    ESP_LOGE(TAG, "idle line LOW before traffic — short to GND / chip holding PD1. Abort.");
    return ESP_FAIL;
  }

  swio_t st = { .pinmask = (1u << gpio), .prog_state = 0 };

  // connect_retry() sends swio_line_reset + connect in a retry loop (the SDI
  // needs a few wakeup attempts before it locks).
  if (connect_retry(&st)) return ESP_FAIL;
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
