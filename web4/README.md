# web4 — XAPOH programmer tool

Standalone browser tool with two tabs:

- **Мост (bridge)** — flash the ESP32-C6 firmware over WebSerial (esptool-js),
  edit the WiFi config, and open a serial console. Same flow as the web3 bridge tab.
- **Экспандеры** — program the 3 CH32V003 expanders. web4 does **not** drive SWIO:
  it sends a 2-byte trigger (`F0`/`F1`/`F2`/`FA`) over WebSerial and the ESP flashes
  its **compiled-in** CH32 image via SWIO on gpio0/1/2 (see `flash_cmd_task` in
  `sw-panel-v4/main/sw-panel-v4-main.c`).

Program mode only (board jumper 3.3 V–3.3 V, ESP on USB).

## Build
One script builds the whole chain and emits a self-contained `web4/index.html`:

```bash
cd web4
npm install
IDF_PATH=/path/to/esp-idf npm run build      # or export IDF_PATH first
```

`bin/build.js` runs, in order:
1. `make -C ../sw-v4 build` — CH32V003 firmware → `sw-v4/main.bin` (compile only).
2. `node bin-to-h.js` in `../sw-panel-v4` — `sw-v4/main.bin` → `main/expander_image.h`
   (the CH32 image compiled into the ESP app).
3. `node page-to-h.js` in `../sw-panel-v4` — hosted UI → `main/pages.h`.
4. `idf.py build` in `../sw-panel-v4` (sources `$IDF_PATH/export.sh`) — ESP app.
5. `sw-panel-v4/build/{bootloader.bin, sw-panel-v4.bin}` → base64 `window.BRIDGE_BINS`.
6. esbuild-bundle `lib/main.js` → `web4/index.html`.

`npm run watch` rebuilds on changes to `lib/`, `sw-v4/main.c`, `sw-panel-v4/lib/`.

### Prerequisites
- **CH32 build**: the cnlohr `ch32fun` toolchain (riscv gcc) — `sw-v4/Makefile`
  includes `../../../../cnlohr/ch32fun/ch32fun/ch32fun.mk`.
- **ESP build**: a working ESP-IDF at `$IDF_PATH` (fallback path hard-coded in
  `bin/build.js`).

### Partition-table note
`bin/build.js` embeds only the bootloader (`0x0`) and app (`0x10000`). The partition
table (`0x8000`) is built **in-browser** by `lib/esp-partition.js` (`t2pt`) because the
firmware needs a `config` data partition (@`0x110000`) that the default IDF singleapp
table lacks. Do not embed the IDF-built `partition-table.bin` — it would omit `config`
and the firmware would assert at boot.

## Use
Serve over http (WebSerial needs https or localhost):

```bash
cd web4 && python3 -m http.server 8000
# open http://localhost:8000 in Chrome/Edge
```

1. **Мост** → Активировать Мост → pick the ESP USB-Serial-JTAG port → edit config →
   Прошить → progress + console.
2. **Экспандеры** → Активировать → pick the port → `Экспандер 0/1/2` or `Все три`
   sends `F0/F1/F2/FA`; the ESP logs SWIO progress back over serial.

## Protocol
2-byte ASCII command: `'F'` then `'0'|'1'|'2'|'A'`. Handled by `flash_cmd_task`
in `sw-panel-v4/main/sw-panel-v4-main.c`.
