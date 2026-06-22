# WCH-LinkE WebUSB flasher — architecture & plan

> **STATUS: implemented and working on hardware** (WCH-LinkE v2.12 + CH32V003).
> `lib/wchlink.js` flashes `window.EXPANDER_IMAGE` end-to-end from `lib/main.js`:
> connect → `programmerInfo` → `chipInfo` → `nrstAsGpio` (unlock+erase) →
> `writeImage` (write + readback verify) → reboot. All milestones 1-6 done;
> only the optional speed work (milestone 7) remains. This doc is kept as the
> reference for *why* the code does what it does.

Goal: a browser-only JS library (`web3/lib/wchlink.js`) that flashes a `.bin`
into a **CH32V003** through a **WCH-LinkE** programmer over **WebUSB**, with no
native helper. Port of the relevant slice of
`ch32fun/minichlink/` (C + libusb) to `navigator.usb`.

Scope is intentionally narrow:

- Programmer: WCH-LinkE in **RISC-V mode** only — `{vendorId: 0x1a86, productId: 0x8010}`.
- Target: **CH32V003** only (the family minichlink calls `CHIP_CH32V003`).
- Operation: **set NRST as GPIO** (also unlocks + mass-erases) + erase + write
  + verify + reboot. No GDB, no terminal, no general option-byte editing, no
  read-protection toggling, no ARM/IAP-mode recovery in v1.

---

## 1. How minichlink is layered (and what we actually need)

`minichlink` splits into a thin **programmer driver** and a fat **generic
algorithm layer**. The driver only has to provide three primitives; everything
else (chip detect, flash unlock, erase, page write) is built on top of them.

| C layer | File | What it does | Port? |
|---|---|---|---|
| Programmer driver | `pgm-wch-linke.c` | USB transfers, DMI reg read/write, power, setup | **yes (the USB part)** |
| Generic algorithm | `minichlink.c` `Default*` fns | chip detect, flash unlock/erase/write via Debug Module | **yes (reimplement in JS)** |
| Chip table | `chips.c` | per-chip sizes/speed | inline the CH32V003 row |

Critical detail: in `pgm-wch-linke.c`, `FORCE_EXTERNAL_CHIP_DETECTION 1`. So the
LinkE driver installs **only**:

```
MCF.WriteReg32 = LEWriteReg32;
MCF.ReadReg32  = LEReadReg32;
MCF.FlushLLCommands = LEFlushLLCommands;   // no-op for LinkE
MCF.SetupInterface = LESetupInterface;
MCF.Control3v3 / Control5v / ResetInterface / Exit / ...
```

`BlockWrite64` is **not** set for LinkE. Therefore flashing flows through the
slow-but-simple `DefaultWriteBinaryBlob` → `DefaultWriteWord` path, which writes
flash **one 32-bit word at a time** by running tiny RISC-V code out of the Debug
Module program buffer. That is the path we port — it needs no WCH bootloader blob
and no `BlockWrite64`.

So the whole job reduces to:

1. **USB plumbing** — `wch_link_command()` (bulk OUT then bulk IN on a vendor interface).
2. **Two DMI primitives** — `LEWriteReg32` / `LEReadReg32` (the `81 08 06 …` packet).
3. **Reimplement the generic Debug-Module algorithm in JS** — setup, halt,
   unlock, erase, word-write loop, wait-for-done, wait-for-flash, reboot.

---

## 2. USB topology / WebUSB mapping

libusb call (C) → WebUSB equivalent:

| minichlink (libusb) | WebUSB |
|---|---|
| `libusb_open` | `await device.open()` |
| (implicit) | `await device.selectConfiguration(1)` |
| `libusb_claim_interface(devh, 0)` | `await device.claimInterface(IFACE)` |
| `libusb_bulk_transfer(devh, 0x01, buf, len, …)` (OUT) | `await device.transferOut(EP_OUT, buf)` |
| `libusb_bulk_transfer(devh, 0x81, buf, len, …)` (IN) | `await device.transferIn(EP_IN, len)` |

Endpoints in C are hardcoded `0x01` OUT and `0x81` IN. The WCH-LinkE is a
**composite device** — and on the real hardware here the vendor interface
(iface 0, `interfaceClass === 0xff`) exposes **two** bulk pairs:

```
if0 cls=255 [in2:bulk, out2:bulk, in1:bulk, out1:bulk]   <- programmer (vendor)
if1 cls=2   [in4:interrupt]                              <- CDC-ACM control
if2 cls=10  [in3:bulk, out3:bulk]                        <- CDC data (serial)
```

So hardcoding `0x01`/`0x81` is wrong twice over (wrong iface possible, and two
candidate pairs on the right iface). The implemented approach:

- Pick the `interfaceClass === 0xff` interface, claim it, then
  `selectAlternateInterface(iface, 0)` (Chrome/Linux needs this before the
  endpoints carry data).
- **Probe** each (out, in) bulk pair with the "get version" handshake and keep
  the pair whose reply starts `0x82 0x0d …`. On this unit the answering pair is
  ep1 (`0x01`/`0x81`), but the probe makes it firmware-agnostic.
- `transferOut(n, …)` / `transferIn(n, …)` take the **endpoint number** (1), not
  the address (`0x81`); direction is implied by the method.

**Priming matters.** A bare version read on a freshly-opened programmer hangs
(the IN never completes). Sending `81 0d 01 ff` (stop) + a short throwaway IN
read first clears that state; *then* the version read answers. The probe does
exactly this stop→drain→version sequence per pair, which doubles as the C
"initial drain" (`pgm-wch-linke.c:264`).

> **No per-call timeout in WebUSB** (libusb had 5 s). Every `transferIn` is
> wrapped in a `Promise.race` timeout so a non-replying endpoint surfaces as an
> error instead of hanging forever.

WebUSB preconditions in `lib/main.js`:
`navigator.usb.requestDevice({filters:[{vendorId:0x1a86,productId:0x8010}]})`
— note WebUSB uses `vendorId`/`productId` (the `usbVendorId`/`usbProductId` keys
are WebSerial). The returned `USBDevice` is passed into
`wchlink({device, onProgress})`.

---

## 3. Protocol reference (the bytes we must emit)

### 3.1 `wchLinkCommand(out)` — control channel (EP1)

Mirrors `wch_link_command()` (`pgm-wch-linke.c:63`): write the command bytes to
bulk OUT, then read one bulk IN packet (≤ ~1024 bytes) as the reply. All control
commands start with `0x81`. Used for setup/power/version.

Commands we need (each is a byte string sent via `transferOut`):

| Bytes | Meaning |
|---|---|
| `81 0d 01 ff` | stop / exit programming mode |
| `81 0d 01 01` | reset programmer state, returns FW version (reply `82 0d 04 <maj> <min> <type>`; type `18`=LinkE) |
| `81 0d 01 02` | put target on hold / connect |
| `81 0c 02 01 02` | set SWD interface speed (default 4 MHz) |
| `81 0c 02 <chip_type> <speed>` | set speed for detected chip (`chip_type=0x09`, `speed=0x01` for V003) |
| `81 0d 01 09` / `81 0d 01 0a` | 3V3 target power on / off |
| `81 0d 01 0b` / `81 0d 01 0c` | 5V target power on / off |
| `81 06 01 01` | query read-protection status (reply byte[3]: 1=protected) |
| `81 0d 01 13` | force reset line low (used in retry dance) |
| `81 0b 01 01` | release reset / part of connect dance |

Note: for the **external-detection** path (what we use) `LESetupInterface`
mostly just runs the connect dance with `81 0d 01 02` and the speed command, then
hands off to the Debug-Module register writes. We can keep our `setupInterface`
minimal (see §5).

### 3.2 DMI register read/write — the workhorse (EP1)

This is the entire low level. From `LEWriteReg32` / `LEReadReg32`
(`pgm-wch-linke.c:294,318`):

```
write reg:  81 08 06 <reg7> <b3> <b2> <b1> <b0> 02      // op 2 = write, value big-endian
read  reg:  81 08 06 <reg7> 00  00  00  00  01          // op 1 = read
reply (9):  82 08 06 <reg7> <b3> <b2> <b1> <b0> <stat>  // value big-endian; stat 0=ok, 2/3=fail
```

`<reg7>` is a 7-bit RISC-V Debug Module register address. Reply must be 9 bytes;
`stat ∈ {2,3}` means error. (If the very first op errors and the programmer was
never initialized, C re-sends `81 0d 01 02` once — `LE_HANDLE_REG_ERROR`.)

So the JS primitives are:

```js
async writeReg32(reg7, val) { … send packet, check 9-byte reply, stat … }
async readReg32(reg7)       { … return (b3<<24)|(b2<<16)|(b1<<8)|b0 … }
```

`FlushLLCommands` is a **no-op** for LinkE (transfers are synchronous), so any
`flush()` calls in the ported algorithm become awaits / nothing.

### 3.3 Debug-Module register map (`minichlink.h:203`)

```
DMDATA0=0x04 DMDATA1=0x05 DMCONTROL=0x10 DMSTATUS=0x11 DMHARTINFO=0x12
DMABSTRACTCS=0x16 DMCOMMAND=0x17 DMABSTRACTAUTO=0x18 DMPROGBUF0..7=0x20..0x27
DMCPBR=0x7C DMCFGR=0x7D DMSHDWCFGR=0x7E DMCHIPID=0x7F
```

`DMCOMMAND` access-register encodings used:
- `0x002310NN` — copy DATA0 → register xNN (NN = 0x0a..0x0f for x10..x15).
- `0x00271008` — write DATA0 → x8 **and** execute program buffer.
- `0x00221008` — read x8 → DATA0.
- `0x00240000` — execute program buffer (no reg transfer).
- `0x00220000|csr` — read a CSR (e.g. marchid `0xf12`) → DATA0.

---

## 4. CH32V003 constants (inline the `chips.c` row)

```js
const CH32V003 = {
  family_id: 0x09,        // chip_type byte for the speed command
  flash_base: 0x08000000,
  flash_size: 16 * 1024,
  sector_size: 64,        // erase + program page granularity
  ram_base: 0x20000000,
  ram_size: 2048,
  interface_speed: 0x01,  // 81 0c 02 09 01
  no_autoexec: false,     // DMABSTRACTAUTO works -> use autoexec word-write
};
```

FLASH peripheral registers (used by the algorithm; addresses are absolute):

```
FLASH_STATR  = 0x4002200C   // bit0 BSY, bit4 WRPRTERR, bit15 BOOT_LOCK
FLASH_CTLR   = 0x40022010
FLASH_ADDR   = 0x40022014
FLASH_KEYR   = 0x40022004   // unlock: 0x45670123 then 0xCDEF89AB
FLASH_OBKEYR = 0x40022008   // 0x45670123 / 0xCDEF89AB
FLASH_MODEKEYR = 0x40022024 // 0x45670123 / 0xCDEF89AB  (fast prog unlock)
```

CTLR bits used: `CR_PAGE_PG=0x00010000 (FTPG)`, `CR_BUF_LOAD=0x00040000`,
`CR_BUF_RST=0x00080000`, `CR_PAGE_ER=0x00020000 (FTER)`, `CR_STRT=0x00000040`,
`CR_MER=0x00000004`.

---

## 5. End-to-end flash sequence (what `flush(bin)` does)

Ported from `DefaultSetupInterface` + `DefaultWriteBinaryBlob` +
`DefaultWriteWord` + `StaticUpdatePROGBUFRegs` + `InternalUnlockFlash` +
`DefaultErase` + `DefaultWaitForFlash` + `DefaultWaitForDoneOp` + `DefaultHaltMode`.

**A. Open & claim** (§2). Drain stale IN.

**B. Programmer setup** (`LESetupInterface`):
1. `81 0d 01 ff` (stop), `81 0d 01 01` (reset state, read version).
2. `81 0c 02 01 02` (default speed).
3. Connect dance: `81 0d 01 02` and retries (we can simplify to a couple tries).
4. `81 0c 02 09 01` (V003 speed).

**C. Debug-Module bring-up** (`DefaultSetupInterface` lines 1271-1310):
```
delay ~16ms
writeReg32(DMSHDWCFGR, 0x5aa50000 | (1<<10))   // x2
writeReg32(DMCFGR,     0x5aa50000 | (1<<10))    // x2
writeReg32(DMCONTROL,  0x80000001)              // x3  (haltreq)
readReg32(DMSTATUS) -> must be != 0x00000000 and != 0xffffffff   (retry 3x)
```

**D. Halt + reset** (`DefaultHaltMode` HALT_AND_RESET):
```
writeReg32(DMSHDWCFGR, 0x5aa50000|(1<<10))
writeReg32(DMCFGR,     0x5aa50000|(1<<10))
writeReg32(DMCONTROL,  0x80000001)
writeReg32(DMCONTROL,  0x80000003)   // reset
writeReg32(DMCONTROL,  0x80000001)   // x2 re-halt
delay 10ms
```

**E. (optional) chip-id sanity** — read marchid via
`DMCOMMAND=0x00220f12` then `DMDATA0`; or read the `0x7f` reg. For v1 we may
skip real detection and just assume CH32V003 (the connect already implies it),
but reading `DMSTATUS`/marchid is a good liveness check.

**F. Unlock flash** (`InternalUnlockFlash`, line 2560):
```
readWord(FLASH_CTLR)                         // see §6 for readWord/writeWord
writeWord(FLASH_KEYR,   0x45670123); writeWord(FLASH_KEYR,   0xCDEF89AB)
writeWord(FLASH_OBKEYR, 0x45670123); writeWord(FLASH_OBKEYR, 0xCDEF89AB)
writeWord(FLASH_MODEKEYR,0x45670123); writeWord(FLASH_MODEKEYR,0xCDEF89AB)
readWord(FLASH_CTLR) -> LOCK bits must be clear
```

**G. Write the blob, sector by sector** (`DefaultWriteBinaryBlob`, no
`BlockWrite64`, V003 branch). For each 64-byte sector covering the image
(pad tail with `0xFF`):
```
if sector not known-erased: Erase(base, 64, type=0)      // §H
writeWord(FLASH_CTLR, CR_PAGE_PG)                        // 0x00010000
writeWord(FLASH_CTLR, CR_BUF_RST | CR_PAGE_PG)           // 0x00090000
waitForFlash()
for j in 0..15:                                          // 16 words = 64 bytes
    writeWord(base + j*4, word_j)   // progbuf word-write w/ autoexec; see §6
writeWord(FLASH_ADDR, base)
writeWord(FLASH_CTLR, CR_PAGE_PG | CR_STRT)              // 0x00010040  -> commit page
waitForFlash()
mark sector not-erased
```

**H. Erase** (`DefaultErase` fast-page, type 0, line 2607):
```
ensure unlocked
writeWord(FLASH_CTLR, CR_PAGE_ER)            // 0x00020000  (FTER)
writeWord(FLASH_ADDR, chunk)
writeWord(FLASH_CTLR, CR_STRT | CR_PAGE_ER)  // 0x00020040
waitForFlash()
```
(Whole-chip erase, type 1: `CTLR=0`, `CTLR=CR_MER`, `CTLR=CR_STRT|CR_MER`,
wait — optional convenience.)

**I. Verify** — `readBinaryBlob` back (§6 read path) and compare. Optional but
cheap and worth it.

**J. Reboot target** (`DefaultHaltMode` HALT_MODE_REBOOT):
```
writeReg32(DMCONTROL, 0x80000001)  // x2
writeReg32(DMCONTROL, 0x80000003)  // reset
writeReg32(DMCONTROL, 0x40000001)  // resumereq
```

**K. Exit** — `81 0d 01 ff`; (optionally power off); `device.releaseInterface` /
`device.close()`.

---

## 6. The word-write / word-read mechanism (most subtle part)

`writeWord(addr,val)` does **not** map to a USB command. It builds a 2-instruction
RISC-V routine in the program buffer and uses the Debug Module's *autoexec* so
that each subsequent `DMDATA0` write re-runs it. From `DefaultWriteWord`
(line 1956) + `StaticUpdatePROGBUFRegs` (line 1763).

**One-time progbuf register prep** (`StaticUpdatePROGBUFRegs`), do once per
write/read "sequence" (state tag changes):
```
readReg32(DMHARTINFO) -> rr ; data0addr = 0xe0000000 | (rr & 0x7ff)
writeReg32(DMABSTRACTAUTO, 0)
writeReg32(DMDATA0, data0addr);     writeReg32(DMCOMMAND, 0x0023100a) // x10 = &DATA0
writeReg32(DMDATA0, data0addr+4);   writeReg32(DMCOMMAND, 0x0023100b) // x11 = &DATA1
writeReg32(DMDATA0, 0x4002200c);    writeReg32(DMCOMMAND, 0x0023100c) // x12 = &FLASH_STATR
writeReg32(DMDATA0, 0x00050000);    writeReg32(DMCOMMAND, 0x0023100d) // x13 = PAGE_PG|BUF_LOAD
```
(`0x00050000` = `CR_PAGE_PG(0x10000) | CR_BUF_LOAD(0x40000)`.)

**Enter write sequence** (first write, or when switching flash↔ram):
```
writeReg32(DMABSTRACTAUTO, 0)
(StaticUpdatePROGBUFRegs if coming from a read/none state)
// progbuf: load val (x8) and addr (x9), store, post-increment addr
writeReg32(DMPROGBUF0, 0x41844100) // c.lw x8,0(x10) ; c.lw x9,0(x11)
writeReg32(DMPROGBUF1, 0x0491c080) // c.sw x8,0(x9) ; c.addi x9,4
// FLASH (V003) tail: ack page-write + spin on BSY:
writeReg32(DMPROGBUF2, 0x0001c184) // c.sw x9,0(x11) ; c.nop
writeReg32(DMPROGBUF3, 0x4200c254) // c.sw x13,4(x12) ; c.lw x8,0(x12)
writeReg32(DMPROGBUF4, 0xfc758805) // c.andi x8,1 ; c.bnez x8,-4   (BSY spin)
writeReg32(DMPROGBUF5, 0x90029002) // c.ebreak ; c.ebreak
writeReg32(DMDATA1, addr)
writeReg32(DMDATA0, val)
writeReg32(DMCOMMAND, 0x00240000)  // execute once
writeReg32(DMABSTRACTAUTO, 1)      // enable autoexec
// flash: waitForDoneOp()
```
For a **non-flash** (RAM) write the progbuf2 is just `0x9002c184`
(`c.sw x9,0(x11); c.ebreak`) and no autoexec/wait needed.

**Subsequent words in the same sequence** (the fast path): just
```
if addr != expected: writeReg32(DMDATA1, addr)
writeReg32(DMDATA0, val)   // autoexec re-runs the routine
if flash: waitForDoneOp()
```
Track `currentstateval` (= next expected addr, advance by 4) and a `statetag`
(`"WRSQ"` vs `"RDSQ"`) exactly like the C, so we know when to rebuild progbuf.

**`readWord(addr)`** (`DefaultReadWord`, line 2479) is the symmetric routine:
prep `x10`, load progbuf with `c.lw x8,0(x10); c.ebreak` style, set `DMDATA0=addr`,
`DMCOMMAND` execute, then `0x00221008` to copy x8→DATA0 and `readReg32(DMDATA0)`.
For flash control registers (`0x40022010`, `0x4002200C`, …) the C has a guarded
path — reading these works the same, just don't enable autoexec across them.

**`waitForDoneOp()`** (line 1232): poll `readReg32(DMABSTRACTCS)` until busy
bit `(1<<12)` clears (timeout 100); if `((cs>>8)&7)!=0` it's a fault — clear with
`writeReg32(DMABSTRACTCS, 0x00000700)` and return error.

**`waitForFlash()`** (line 1201): poll `readWord(FLASH_STATR)` until `& 3 == 0`
(BSY clear), timeout ~1000; if `& WRPRTERR(0x10)` → protection error.

> Implementation tip: this autoexec/state-tag bookkeeping is the part most likely
> to have bugs. Start with a **dumb, correct** variant — rebuild the progbuf and
> issue an explicit `DMCOMMAND=0x00240000` execute for *every* word, no autoexec,
> `waitForDoneOp` after each. It's slower (16 KB still flashes in a couple
> seconds) but far easier to get right. Add autoexec batching only once the dumb
> path verifies.

---

## 6b. NRST as GPIO (the pre-flash unlock)

The CH32V003 NRST pin (PD7) can be reclaimed as GPIO by clearing the `RST_MODE`
bit in the USER option byte. We do this **before flashing** — and as a side
effect the option-byte rewrite resets `RDPR` to unprotected (`0xA5`) and triggers
a **full chip mass-erase**, so the one command also serves as the unlock that
lets a factory-fresh (read-protected) chip be programmed.

> Naming note: this is minichlink's `-D` (`minichlink.c:363` →
> `MCF.ConfigureNRSTAsGPIO`). It is **not** the read-protection `-p`/`-P` flags
> (`ConfigureReadProtection`) — we don't use those.

> WARNING: this writes option bytes and therefore **mass-erases flash**. Existing
> firmware is destroyed. That's intended here (we flash right after).

### What runs (LinkE firmware-assisted)

For CH32V003 the LinkE driver sets `MCF.ConfigureNRSTAsGPIO =
LEConfigureNRSTAsGPIO` (`pgm-wch-linke.c:718`). The firmware does the option-byte
erase + rewrite internally — two control commands
(`LEConfigureNRSTAsGPIO`, `pgm-wch-linke.c:640`):

```
81 06 08 02 ff ff ff ff ff ff ff     // write option bytes, op 02, USER=0xff -> NRST as GPIO
81 0b 01 01                          // apply + reset
```

Byte layout of the first command: `81 06 <len=08> <op=02> <USER> <other OB...>`.
`USER=0xff` selects NRST-as-GPIO; `USER=0xf7` keeps NRST as reset (the
`one_if_yes_gpio` arg toggles exactly this byte). No `81 11 01 09` prefix is
needed (that prefix belongs to the read-protection path, which we dropped).

### Sequence around it

1. `HaltMode(HALT_AND_RESET)` (§D).
2. The two commands above.
3. **Re-init.** Chip was mass-erased and reset; option bytes latch on reset.
   Re-run §B→§D before programming.
4. Proceed to unlock/erase/write (§F→).

---

## 7. Proposed JS module shape

Entrypoint (as implemented): `export const wchlink = async ({device, onProgress})`
returning `{programmerInfo, chipInfo, nrstAsGpio, writeImage, close}`. Internally
split into small closures matching the layers:

```
lib/wchlink.js            // public: wchlink({device, onProgress}) -> { programmerInfo, chipInfo, nrstAsGpio, writeImage, close }
  └ transport: open/claim/drain, transferOut, transferIn(+timeout)
  └ link:      wchLinkCommand(bytes), setSpeed, power3v3, version, connectDance
  └ dmi:       writeReg32(reg,val), readReg32(reg)         // §3.2
  └ dm:        setupDM, halt(mode), readWord, writeWord, waitForDoneOp, waitForFlash   // §6
  └ option:    nrstAsGpio() (2 cmds + re-init; also unlocks + mass-erases)   // §6b
  └ flash:     unlock, eraseSector, writeImage(bytes), verify   // §5
  └ writeImage(bytes): orchestrate F→K, report progress per sector
```

Helpers: a `u32le`/`u32be` pack/unpack, a `delay(ms)`, and a constants module.
`bin` arrives as `ArrayBuffer`/`Uint8Array` (from `fetch`/file input); base is
`0x08000000`; pad to 64-byte multiple with `0xFF`.

`onProgress({phase, done, total})` so `main.js` can drive the xterm/console UI
that's already wired in the Expander tab.

---

## 8. WebUSB gotchas / risks (and how they bit us)

- **No transfer timeout.** `transferIn` blocks until data or stall. Every read is
  wrapped in a `Promise.race` timeout; on timeout we surface an error (or ignore,
  for the probe/drain). *This was the original "blank console" hang.*
- **Endpoint discovery, not hardcoding** — composite device with TWO bulk pairs on
  the vendor iface (§2). We probe + lock the pair that answers the version
  handshake. *Hardcoding ep1 worked here, but the probe is the safe path.*
- **Priming the programmer** (§2) — a bare version read on a fresh open hangs;
  `81 0d 01 ff` + a throwaway IN read first unsticks it.
- **`selectAlternateInterface(iface, 0)`** after claim — Chrome/Linux often needs
  it before endpoints carry data.
- **Reply framing.** Each control command expects exactly one IN packet; for DMI
  ops the reply is 9 bytes. Read with a generous length (e.g. 64) and slice.
- **`device.open()` can throw** if the OS/another tab holds the device. On Linux
  the kernel `cdc_acm` driver may claim the *CDC* interface (a different iface —
  harmless), but a udev rule (`web2/99-minichlink.rules` equivalent) is typically
  needed so the browser can access the *vendor* iface.
- **Reset semantics differ.** WebUSB has no `libusb_reset_device`; rely on the
  `81 0d 01 ff` / `DMCONTROL` reset commands only.
- **Endianness:** DMI value bytes are **big-endian on the wire** (§3.2), but the
  flash image and FLASH register values are little-endian RISC-V words. Keep the
  two conversions separate and well-named.
- **No ARM/IAP recovery** in v1: if the LinkE is in ARM mode (pid `0x8012`) or IAP
  (`4348:55e0`), `requestDevice` with our filter won't even find it. Document that
  the user must use it in RISC-V mode.

---

## 9. Milestones

1. ✅ **Transport + link** — open/claim/probe, `cmd()`, FW version (`81 0d 01 01`).
2. ✅ **DMI primitives** — `writeReg32`/`readReg32`; DM bring-up (§C), halt/reset.
3. ✅ **NRST as GPIO** (§6b) — 2-command option-byte write + re-init (unlocks +
   mass-erases). Runs before flashing.
4. ✅ **readWord** — used by `chipInfo` (UUID) and by verify.
5. ✅ **Erase + dumb writeWord** (§6 tip) — per-sector erase + word write.
6. ✅ **Full `writeImage(bytes)`** — sector loop, pad to 64 B, progress, readback
   verify, reboot. Confirmed on hardware.
7. ⬜ **Optimize** (optional) — autoexec batching, fewer round-trips. Only worth it
   for large images; the "dumb" per-word path is fine for the small Expander fw.

Implemented in `lib/wchlink.js`, driven from `lib/main.js` (Expander tab), which
wires `onProgress` into the in-page console.

---

## 10. Reference index (line numbers in `ch32fun/minichlink/`)

- `pgm-wch-linke.c:63` `wch_link_command` — USB OUT/IN framing
- `pgm-wch-linke.c:294` `LEWriteReg32`, `:318` `LEReadReg32` — DMI packet (§3.2)
- `pgm-wch-linke.c:360` `LESetupInterface` — connect dance / speed
- `pgm-wch-linke.c:606/617` `LEControl3v3/5v` — target power
- `pgm-wch-linke.c:640` `LEConfigureNRSTAsGPIO` — NRST-as-GPIO 2-cmd (§6b)
- `minichlink.c:363` `-D` arg → `ConfigureNRSTAsGPIO(dev,1)` (§6b)
- `minichlink.c:1271` `DefaultSetupInterface` — DM bring-up (§C)
- `minichlink.c:1763` `StaticUpdatePROGBUFRegs` — progbuf reg prep (§6)
- `minichlink.c:1956` `DefaultWriteWord` — word write via progbuf+autoexec (§6)
- `minichlink.c:2479` `DefaultReadWord` — word read (§6)
- `minichlink.c:2070` `DefaultWriteBinaryBlob` — sector loop (§G)
- `minichlink.c:2560` `InternalUnlockFlash` — key sequence (§F)
- `minichlink.c:2607` `DefaultErase` — page/whole erase (§H)
- `minichlink.c:1201` `DefaultWaitForFlash`, `:1232` `DefaultWaitForDoneOp`
- `minichlink.c:3101` `DefaultHaltMode` — halt/reset/reboot (§D,§J)
- `chips.c:3` `ch32v003` row — sizes/speed (§4)
- `minichlink.h:203` DM register defines (§3.3)
</content>
</invoke>
