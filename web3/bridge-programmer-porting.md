# Finish ESP32-C6 bridge programmer in web3

## Context

The ESP32-C6 ("Мост"/bridge) programmer is being migrated from the old Observable
app (`src/components/esp-install.js`) into the standalone `web3/` build
(`web3/lib/main.js` + esbuild → single `index.html`). The CH32V003 expander side
(`web3/lib/wchlink.js`) is already complete. The bridge side is only half-ported:
it connects to the chip but cannot flash. Goal: embed the ESP32 firmware binaries
into the build (like the existing `EXPANDER_IMAGE`) and make the bridge flash all
partitions exactly as `esp-install.js` did.

Two existing bugs block the current bridge path (found in review):
- `web3/lib/main.js:16` uses `xterm.callbacks`, but `xterm` is a factory
  function (`web3/lib/xterm.js:5`), so `.callbacks` is `undefined`.
- `esptool-js` is imported but missing from `web3/package.json` deps.

## Approach

Partition table is generated **at runtime** in the browser (port `t2pt` + `js-md5`),
matching `esp-install.js`. Erase is **per-region** (`eraseAll: false`).

### 1. `web3/package.json` — deps
Add to `devDependencies` (esbuild bundles them):
- `esptool-js` (already imported, currently missing)
- `js-md5` (needed by `t2pt`)

### 2. `web3/lib/esp-partition.js` — NEW (ported helpers)
Port from `src/components/`, browser-import style (no `npm:` prefix):
- `t2pt` — verbatim from `src/components/t2pt.js`.
- `partTable` — the 4-entry table from `esp-install.js:70` (nvs, phy_init,
  factory, **config@0x110000**).
- `cfg2ui8(cfg)` — from `esp-install.js:79`, but read **nested** config:
  `cfg?.wifi?.ssid` / `cfg?.wifi?.password` (editor emits `{wifi:{ssid,password}}`,
  see `web3/lib/mount-bridge-config-editor.js:1`). ssid→offset 0, password→offset 32.
- `ui8ToBstr` helper (binary string for esptool `data`).

### 3. `web3/bin/build.js` — embed bridge bins (bin2js functionality)
Mirror the existing `getExpanderImage` pattern. Read from `../sw-panel-v3/build/`
(authoritative paths in `flasher_args.json`):
- `bootloader/bootloader.bin` → address `0x0`
- `sw-panel-v3.bin` → address `0x10000`

Emit a `BRIDGE_BINS` array of `{name, address, data: <base64>}` and inject into the
HTML template next to `window.EXPANDER_IMAGE`:
```js
<script>window.BRIDGE_BINS = ${JSON.stringify(bridgeBins)};</script>
```
(Do NOT embed IDF's `partition_table/partition-table.bin` — it lacks the config
partition; the table is built at runtime instead.)

### 4. `web3/lib/main.js` — `genOnClickActivateBridge`
- Fix xterm: `const term = xterm();` once; use `term.callbacks` for
  `flashOptions.terminal` (line 16) and `term.div` for the console (line 43).
- Keep `{port, transport, esploader, chip}` from connect; render panel + config
  editor + flash button (existing markup). Fix double-`v` in button label
  (`bridge-fw-${pkgVersion}`).
- Wire flash button handler (replace commented lines 42–43). On click, build
  `fileArray` like `esp-install.js:97` `onProgramClick`:
  1. partition table: `{address: 0x8000, data: ui8ToBstr(await t2pt(partTable, (v)=>new Uint8Array(md5.arrayBuffer(v))))}`
  2. firmware: `window.BRIDGE_BINS.map(e => ({address: e.address, data: atob(e.data)}))`
  3. config: `{address: 0x110000, data: ui8ToBstr(cfg2ui8(cfg))}` — `cfg` parsed
     from the editor's localStorage `bridgeConfig` JSON.
  Then `esploader.writeFlash({fileArray, flashSize:'keep', eraseAll:false, compress:true, reportProgress})`,
  `await esploader.after()`, append `term.div` to `#console`, run the
  `transport.rawRead()` → `term.write(value)` console loop (from
  `esp-install.js:130`).
- `reportProgress(fileIndex, written, total)` → write progress line into the panel.

## Verification
- `cd web3 && npm install` (pulls esptool-js, js-md5).
- `cd web3 && npm run build` → regenerates `index.html`; confirm `window.BRIDGE_BINS`
  present with 2 entries (bootloader@0x0, app@0x10000) and base64 data.
- `cd web3 && npm test` (eslint bin + lib) passes.
- Manual (Chrome, WebSerial): open `index.html`, "Мост" tab → Активировать →
  chip info shows → edit wifi config → flash button → progress for all 3 regions →
  device reboots → console shows ESP32 boot log.
