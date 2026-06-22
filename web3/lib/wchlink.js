// WCH-LinkE programmer over WebUSB. Target: CH32V003.
// Port of the relevant slice of ch32fun/minichlink. See wchlink-webusb.md.

// --- Debug Module register addresses (minichlink.h) ---
const DMDATA0 = 0x04;
const DMDATA1 = 0x05;
const DMCONTROL = 0x10;
const DMSTATUS = 0x11;
const DMHARTINFO = 0x12;
const DMABSTRACTCS = 0x16;
const DMCOMMAND = 0x17;
const DMABSTRACTAUTO = 0x18;
const DMPROGBUF0 = 0x20;
const DMPROGBUF1 = 0x21;
const DMPROGBUF2 = 0x22;
const DMPROGBUF3 = 0x23;
const DMPROGBUF4 = 0x24;
const DMPROGBUF5 = 0x25;
const DMCFGR = 0x7d;
const DMSHDWCFGR = 0x7e;

// --- CH32V003 flash peripheral (absolute addresses) ---
const FLASH_KEYR = 0x40022004;
const FLASH_OBKEYR = 0x40022008;
const FLASH_STATR = 0x4002200c;
const FLASH_CTLR = 0x40022010;
const FLASH_ADDR = 0x40022014;
const FLASH_MODEKEYR = 0x40022024;

const KEY1 = 0x45670123;
const KEY2 = 0xcdef89ab;

// FLASH_CTLR bits
const CR_PAGE_PG = 0x00010000; // FTPG
const CR_PAGE_ER = 0x00020000; // FTER
const CR_BUF_LOAD = 0x00040000;
const CR_BUF_RST = 0x00080000;
const CR_STRT = 0x00000040;

const FLASH_BASE = 0x08000000;
const SECTOR = 64; // CH32V003 page / erase / program granularity
const CHIP_TYPE = 0x09; // CH32V003 family id (for the speed command)
const IFACE_SPEED = 0x01;

const CFGR_KEY = 0x5aa50000 | (1 << 10); // allow output from slave

const delay = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
const hex = (n) => '0x' + (n >>> 0).toString(16).padStart(8, '0');
const dashHex = (arr) => Array.from(arr, (b) => b.toString(16).padStart(2, '0')).join('-');

// addr is flash if top 3 bits are clear (minichlink IsAddressFlash)
const isAddressFlash = (addr) => ((addr & 0xe0000000) >>> 0) === 0;

const withTimeout = (promise, ms, label) => Promise.race([
  promise,
  new Promise((_, reject) => setTimeout(() => reject(new Error(`${label} timeout ${ms}ms`)), ms))
]);

export const wchlink = async (options) => { // eslint-disable-line complexity
  const {device, onProgress = () => {}} = options;

  // --- transport ------------------------------------------------------------
  await device.open();
  if (device.configuration === null) {
    await device.selectConfiguration(1);
  }

  // Pick the vendor (class 0xff) interface; collect ALL its bulk endpoints.
  // WCH-LinkE iface 0 exposes two bulk pairs (ep1 + ep2); which one carries
  // commands varies by firmware, so we probe below.
  let vendorIntf = null;
  for (const intf of device.configuration.interfaces) {
    for (const alt of intf.alternates) {
      const hasBulk = alt.endpoints.some((e) => e.type === 'bulk');
      if (hasBulk && (vendorIntf === null || alt.interfaceClass === 0xff)) {
        vendorIntf = {number: intf.interfaceNumber, alt};
        if (alt.interfaceClass === 0xff) break;
      }
    }
    if (vendorIntf && vendorIntf.alt.interfaceClass === 0xff) break;
  }
  if (!vendorIntf) throw new Error('WCH-LinkE: no bulk interface found');

  const ifaceNumber = vendorIntf.number;
  const insBulk = vendorIntf.alt.endpoints.filter((e) => e.type === 'bulk' && e.direction === 'in').map((e) => e.endpointNumber);
  const outsBulk = vendorIntf.alt.endpoints.filter((e) => e.type === 'bulk' && e.direction === 'out').map((e) => e.endpointNumber);

  await device.claimInterface(ifaceNumber);
  // Some platforms (Chrome/Linux) need the alt setting selected before transfers.
  try { await device.selectAlternateInterface(ifaceNumber, 0); } catch (e) { /* optional */ }

  // Probe out/in bulk pairs: the command channel is the one that answers the
  // "get version" command (reply starts 0x82 0x0d ...).
  let epIn = null;
  let epOut = null;
  for (const o of outsBulk) {
    for (const i of insBulk) {
      try {
        await device.transferOut(o, new Uint8Array([0x81, 0x0d, 0x01, 0xff])); // stop
        try { await withTimeout(device.transferIn(i, 64), 150, 'p'); } catch (e) { /* maybe silent */ }
        await device.transferOut(o, new Uint8Array([0x81, 0x0d, 0x01, 0x01])); // version
        const rr = await withTimeout(device.transferIn(i, 16), 400, 'p');
        const u = new Uint8Array(rr.data.buffer, rr.data.byteOffset, rr.data.byteLength);
        if (u.length >= 3 && u[0] === 0x82) {
          epOut = o;
          epIn = i;
        }
      } catch (e) { /* this pair didn't answer */ }
      if (epIn !== null) break;
    }
    if (epIn !== null) break;
  }
  if (epIn === null) {
    throw new Error(`WCH-LinkE: no responding bulk pair (out=[${outsBulk}] in=[${insBulk}])`);
  }
  onProgress({phase: `USB: iface ${ifaceNumber}, EP in=${epIn} out=${epOut}`});

  const transferOut = (bytes) => device.transferOut(epOut, new Uint8Array(bytes));
  const transferIn = async (len = 64, ms = 3000) => {
    const r = await withTimeout(device.transferIn(epIn, len), ms, 'USB read');
    return new Uint8Array(r.data.buffer, r.data.byteOffset, r.data.byteLength);
  };

  // --- link control channel (0x81 ... commands) ----------------------------
  // Every command is OUT then a single IN reply (matches wch_link_command).
  const cmd = async (bytes, replyLen = 64) => {
    await transferOut(bytes);
    return transferIn(replyLen);
  };

  // --- DMI register read/write (the 81 08 06 ... packet) --------------------
  const writeReg32 = async (reg7, val) => {
    const v = val >>> 0;
    await cmd([
      0x81, 0x08, 0x06, reg7 & 0x7f,
      (v >>> 24) & 0xff, (v >>> 16) & 0xff, (v >>> 8) & 0xff, v & 0xff,
      0x02 // op 2 = write
    ], 16);
  };
  const readReg32 = async (reg7) => {
    const r = await cmd([0x81, 0x08, 0x06, reg7 & 0x7f, 0, 0, 0, 0, 0x01], 16);
    return ((r[4] << 24) | (r[5] << 16) | (r[6] << 8) | r[7]) >>> 0;
  };

  // --- Debug Module helpers -------------------------------------------------
  let progState = null;   // 'flash' | 'reg' | null — current program-buffer shape
  let flashUnlocked = false;

  const waitForDoneOp = async () => {
    let cs = 0;
    let t = 100;
    do {
      cs = await readReg32(DMABSTRACTCS);
    } while ((cs & (1 << 12)) && t-- > 0);
    if (((cs >> 8) & 7) || (cs & (1 << 12))) {
      await writeReg32(DMABSTRACTCS, 0x00000700); // clear errors
      throw new Error('Debug Module abstract-command fault ' + hex(cs));
    }
  };

  // Read a 32-bit word from target memory. Clobbers x8 / PROGBUF0.
  const readWord = async (addr) => {
    await writeReg32(DMABSTRACTAUTO, 0);
    await writeReg32(DMPROGBUF0, 0x90024000); // c.lw x8,0(x8) ; c.ebreak
    await writeReg32(DMDATA0, addr >>> 0);
    await writeReg32(DMCOMMAND, 0x00271008);  // DATA0 -> x8, execute
    await waitForDoneOp();
    await writeReg32(DMCOMMAND, 0x00221008);  // x8 -> DATA0
    const v = await readReg32(DMDATA0);
    progState = null; // program buffer was repurposed
    return v >>> 0;
  };

  const waitForFlash = async () => {
    let rw = 0;
    let t = 1000;
    do {
      rw = await readWord(FLASH_STATR);
    } while ((rw & 3) && t-- > 0);
    if (rw & 0x10) throw new Error('Flash write-protect error (read protected?)');
    if (t <= 0) throw new Error('Flash timeout, STATR=' + hex(rw));
  };

  // Load x10=&DATA0, x11=&DATA1, x12=&FLASH_STATR, x13=PAGE_PG|BUF_LOAD.
  const updateProgbufRegs = async () => {
    const rr = await readReg32(DMHARTINFO);
    const data0addr = (0xe0000000 | (rr & 0x7ff)) >>> 0;
    await writeReg32(DMABSTRACTAUTO, 0);
    await writeReg32(DMDATA0, data0addr);             await writeReg32(DMCOMMAND, 0x0023100a);
    await writeReg32(DMDATA0, (data0addr + 4) >>> 0); await writeReg32(DMCOMMAND, 0x0023100b);
    await writeReg32(DMDATA0, FLASH_STATR);           await writeReg32(DMCOMMAND, 0x0023100c);
    await writeReg32(DMDATA0, CR_PAGE_PG | CR_BUF_LOAD); await writeReg32(DMCOMMAND, 0x0023100d);
  };

  // Build the store routine in the program buffer for flash vs. plain memory.
  const ensureWriteProg = async (flash) => {
    const want = flash ? 'flash' : 'reg';
    if (progState === want) return;
    await writeReg32(DMABSTRACTAUTO, 0);
    await updateProgbufRegs();
    await writeReg32(DMPROGBUF0, 0x41844100); // c.lw x8,0(x10) ; c.lw x9,0(x11)
    await writeReg32(DMPROGBUF1, 0x0491c080); // c.sw x8,0(x9)  ; c.addi x9,4
    if (flash) {
      // ack page-write (BUF_LOAD via x13) then spin on STATR BSY
      await writeReg32(DMPROGBUF2, 0x0001c184); // c.sw x9,0(x11) ; c.nop
      await writeReg32(DMPROGBUF3, 0x4200c254); // c.sw x13,4(x12); c.lw x8,0(x12)
      await writeReg32(DMPROGBUF4, 0xfc758805); // c.andi x8,1    ; c.bnez x8,-4
      await writeReg32(DMPROGBUF5, 0x90029002); // c.ebreak       ; c.ebreak
    } else {
      await writeReg32(DMPROGBUF2, 0x9002c184); // c.sw x9,0(x11) ; c.ebreak
    }
    progState = want;
  };

  // Write one 32-bit word to target memory (flash or peripheral register).
  // "Dumb" mode: explicit execute per word, no autoexec (slower but robust).
  const writeWord = async (addr, val) => {
    const flash = isAddressFlash(addr);
    await ensureWriteProg(flash);
    await writeReg32(DMDATA1, addr >>> 0);
    await writeReg32(DMDATA0, val >>> 0);
    await writeReg32(DMCOMMAND, 0x00240000); // execute program buffer
    if (flash) await waitForDoneOp();
  };

  // --- programmer / DM bring-up ---------------------------------------------
  const halt = async () => {
    await writeReg32(DMSHDWCFGR, CFGR_KEY);
    await writeReg32(DMCFGR, CFGR_KEY);
    await writeReg32(DMCONTROL, 0x80000001);
    await writeReg32(DMCONTROL, 0x80000003); // reset
    await writeReg32(DMCONTROL, 0x80000001); // re-halt
    await writeReg32(DMCONTROL, 0x80000001);
    await delay(10);
    progState = null;
    flashUnlocked = false;
  };

  const setupDM = async () => {
    let tries = 3;
    for (;;) {
      await delay(16);
      await writeReg32(DMSHDWCFGR, CFGR_KEY);
      await writeReg32(DMCFGR, CFGR_KEY);
      await writeReg32(DMSHDWCFGR, CFGR_KEY);
      await writeReg32(DMCFGR, CFGR_KEY);
      await writeReg32(DMCONTROL, 0x80000001);
      await writeReg32(DMCONTROL, 0x80000001);
      await writeReg32(DMCONTROL, 0x80000001);
      const st = await readReg32(DMSTATUS);
      if (st !== 0x00000000 && st !== 0xffffffff) return;
      if (--tries <= 0) throw new Error('Debug Module setup failed (DMSTATUS=' + hex(st) + ')');
    }
  };

  let connected = false;
  const connect = async () => {
    await cmd([0x81, 0x0d, 0x01, 0xff]);       // stop
    await cmd([0x81, 0x0d, 0x01, 0x01], 16);   // reset state / version
    await cmd([0x81, 0x0c, 0x02, 0x01, 0x02]); // default speed
    await cmd([0x81, 0x0d, 0x01, 0x02]);       // connect / hold target
    await cmd([0x81, 0x0c, 0x02, CHIP_TYPE, IFACE_SPEED]); // V003 speed
    await setupDM();
    await halt();
    connected = true;
  };
  const ensureConnected = async () => {
    if (!connected) await connect();
  };

  // --- flash operations -----------------------------------------------------
  const readProtected = async () => {
    const r = await cmd([0x81, 0x06, 0x01, 0x01], 16); // query read-protection
    return r.length >= 4 && r[3] === 0x01;
  };

  const unlockFlash = async () => {
    if (flashUnlocked) return;
    await readWord(FLASH_CTLR);
    await writeWord(FLASH_KEYR, KEY1);
    await writeWord(FLASH_KEYR, KEY2);
    await writeWord(FLASH_OBKEYR, KEY1);
    await writeWord(FLASH_OBKEYR, KEY2);
    await writeWord(FLASH_MODEKEYR, KEY1);
    await writeWord(FLASH_MODEKEYR, KEY2);
    const ctlr = await readWord(FLASH_CTLR);
    if (ctlr & 0x80) throw new Error('Flash unlock failed (CTLR=' + hex(ctlr) + ')'); // LOCK still set
    flashUnlocked = true;
  };

  const eraseSector = async (base) => {
    await writeWord(FLASH_CTLR, CR_PAGE_ER);
    await writeWord(FLASH_ADDR, base >>> 0);
    await writeWord(FLASH_CTLR, CR_STRT | CR_PAGE_ER);
    await waitForFlash();
  };

  const reboot = async () => {
    await writeReg32(DMCONTROL, 0x80000001);
    await writeReg32(DMCONTROL, 0x80000001);
    await writeReg32(DMCONTROL, 0x80000003); // reset
    await writeReg32(DMCONTROL, 0x40000001); // resumereq
  };

  // --- public API -----------------------------------------------------------
  const programmerInfo = async () => { // WCH-LinkE info
    // stop/exit — a fresh programmer may not reply to this; tolerate a timeout
    try {
      await transferOut([0x81, 0x0d, 0x01, 0xff]);
      await transferIn(64, 300);
    } catch (e) { /* no reply to stop — fine */ }
    const r = await cmd([0x81, 0x0d, 0x01, 0x01], 16); // reply: 82 0d 04 <maj> <min> <type>
    const types = {1: 'CH549', 2: 'CH32V307', 3: 'CH32V203', 4: 'LinkB', 5: 'LinkW', 18: 'WCH-LinkE'};
    return {
      name: types[r[5]] || 'unknown',
      typeCode: r[5],
      version: `${r[3]}.${r[4]}`
    };
  };

  const chipInfo = async () => { // CH32V003 info
    await ensureConnected();
    const r = await cmd([0x81, 0x11, 0x01, 0x05], 32); // 20-byte chip status
    const flashKb = (r[2] << 8) | r[3];
    return {
      name: 'CH32V003',
      flashSize: (flashKb || 16) * 1024,
      uuid: dashHex(r.slice(4, 12)),  // e.g. c3-6a-ab-cd-2b-48-bc-11
      type: dashHex(r.slice(16, 20)), // e.g. 00-30-05-00
      protected: await readProtected()
    };
  };

  // Configure NRST as GPIO via option bytes (minichlink -D / LEConfigureNRSTAsGPIO).
  // The option-byte write also resets RDPR to unprotected and mass-erases flash,
  // so this doubles as the pre-flash unlock.
  const nrstAsGpio = async () => {
    await ensureConnected();
    onProgress({phase: 'NRST → GPIO (стирает чип целиком)'});
    await halt();
    // op 02 = write option bytes; USER=0xff sets NRST as GPIO (0xf7 would keep reset).
    await cmd([0x81, 0x06, 0x08, 0x02, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff]);
    await cmd([0x81, 0x0b, 0x01, 0x01]); // apply + reset
    await delay(20);
    // Chip was mass-erased and reset; option bytes latch on reset — reconnect.
    connected = false;
    await connect();
  };

  const writeImage = async (bin) => { // write binary image into flash @ 0x08000000
    await ensureConnected();
    await halt();
    await unlockFlash();

    // pad to a whole number of 64-byte sectors with 0xFF
    const total = Math.ceil(bin.length / SECTOR);
    const padded = new Uint8Array(total * SECTOR).fill(0xff);
    padded.set(bin);

    for (let s = 0; s < total; s++) {
      const base = (FLASH_BASE + s * SECTOR) >>> 0;
      await eraseSector(base);
      await writeWord(FLASH_CTLR, CR_PAGE_PG);
      await writeWord(FLASH_CTLR, CR_BUF_RST | CR_PAGE_PG);
      await waitForFlash();
      for (let j = 0; j < SECTOR / 4; j++) {
        const o = s * SECTOR + j * 4;
        const w = (padded[o] | (padded[o + 1] << 8) | (padded[o + 2] << 16) | (padded[o + 3] << 24)) >>> 0;
        await writeWord((base + j * 4) >>> 0, w);
      }
      await writeWord(FLASH_ADDR, base);
      await writeWord(FLASH_CTLR, CR_PAGE_PG | CR_STRT); // commit page
      await waitForFlash();
      onProgress({phase: 'Прошивка', done: s + 1, total});
    }

    // verify: read flash back and compare to the image
    for (let s = 0; s < total; s++) {
      for (let j = 0; j < SECTOR / 4; j++) {
        const o = s * SECTOR + j * 4;
        const expected = (padded[o] | (padded[o + 1] << 8) | (padded[o + 2] << 16) | (padded[o + 3] << 24)) >>> 0;
        const got = await readWord((FLASH_BASE + o) >>> 0);
        if (got !== expected) {
          throw new Error(`Проверка не прошла @ ${hex(FLASH_BASE + o)}: ${hex(got)} != ${hex(expected)}`);
        }
      }
      onProgress({phase: 'Проверка', done: s + 1, total});
    }

    await reboot();
    onProgress({phase: 'Готово', done: total, total});
  };

  const close = async () => {
    try { await device.releaseInterface(ifaceNumber); } catch (e) { /* ignore */ }
    await device.close();
  };

  return {programmerInfo, chipInfo, nrstAsGpio, writeImage, close};
};
