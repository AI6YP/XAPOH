#!/usr/bin/env node
import { readFile, writeFile } from 'fs/promises';
import { existsSync } from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';
import childProcess from 'child_process';

import { swV4ToH } from './sw-v4-to-h.js';
import { bundleCtrlWebAppToH } from './bundle-ctrl-web-app-to-h.js';
import { bundleWeb4App, web4HtmlTemplate } from './bundle-web4-app.js';

const getVersion = () => {
  const now = new Date();
  const year = now.getFullYear();
  // Months are zero-indexed, so we add 1
  const month = String(now.getMonth() + 1).padStart(2, '0');
  const day = String(now.getDate()).padStart(2, '0');
  const hours = String(now.getHours()).padStart(2, '0');
  const minutes = String(now.getMinutes()).padStart(2, '0');
  const res = 'v9 ' + year + '-' + month + '-' + day + ' ' + hours + ':' + minutes;
  return res;
};

const manifest = {version: getVersion()};

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(__dirname, '..');           // web4/
const repo = path.resolve(root, '..');                // XAPOH/
const swV4 = path.join(repo, 'sw-v4');                // ch32v003 fw
const swPanelV4 = path.join(repo, 'sw-panel-v4');     // esp32c6 fw / app

// IDF: honour $IDF_PATH, else fall back to the known checkout.
const IDF_PATH = process.env.IDF_PATH || '/home/drom/work/github/espressif/esp-idf';

const run = (cmd, args, cwd) => new Promise((resolve, reject) => {
  console.log(`\n$ (${cwd}) ${cmd} ${args.join(' ')}`); // eslint-disable-line no-console
  const child = childProcess.spawn(cmd, args, {cwd, stdio: 'inherit'});
  child.on('exit', (code) =>
    code === 0 ? resolve() : reject(new Error(`${cmd} exited with code ${code}`)));
  child.on('error', reject);
});

// --- step 5: ESP bridge bins (bootloader + app only; NOT the idf PT) -----------
const bridgeFiles = [
  {address: 0x0,     file: 'bootloader/bootloader.bin'},
  {address: 0x10000, file: 'sw-panel-v4.bin'}
];

const getBridgeBins = async () => {
  const buildDir = path.join(swPanelV4, 'build');
  const bins = [];
  for (const {address, file} of bridgeFiles) {
    const data = (await readFile(path.join(buildDir, file))).toString('base64');
    bins.push({name: path.basename(file), address, data});
  }
  return bins;
};

const main = async () => {
// web4/bin/build.js — one script, whole chain:

  // 1. CH32V003 firmware. `build` target = compile only; the default `flash`
  //    target would invoke minichlink and fail without a programmer attached.
  // -> sw-v4/main.bin
  console.log('1 ====================');
  await run('make', ['-C', swV4, 'build'], repo);

  // 2. CH32 bin->h (must precede the ESP build: compiled in)
  // sw-v4/main.bin -> main/expander_image.h
  console.log('2 ====================');
  const swV4data = await readFile(path.join(swV4, 'main.bin'));
  await writeFile(path.join(swPanelV4, 'main', 'expander_image.h'), swV4ToH(swV4data));
  console.log('expander_image.h written (%d bytes)', swV4data.length); // eslint-disable-line no-console

  // 3. esp32c6 hosted UI build (must precede the ESP build)
  // -> sw-panel-v4/main/pages.h
  console.log('3 ====================');
  await bundleCtrlWebAppToH(swPanelV4, manifest);

  // 4. ESP32-C6 C app (needs a sourced IDF environment)
  //    compiles in the CH32 image
  // -> sw-panel-v4/build/*.bin
  console.log('4 ====================');
  if (!existsSync(path.join(IDF_PATH, 'export.sh'))) {
    throw new Error(`IDF export.sh not found at ${IDF_PATH}. Set IDF_PATH.`);
  }
  await run('bash', ['-c', `source "${IDF_PATH}/export.sh" >/dev/null && idf.py build`], swPanelV4);

  // 5. ESP bins  -> base64 (bootloader @0x0, app @0x10000; NOT the idf partition
  //                   table — web4 builds a table WITH a config partition in-browser)
  console.log('5 ====================');
  const bridgeBins = await getBridgeBins();

  // 6. bundle web4/lib/main.js with esbuild -> self-contained web4/index.html
  console.log('6 ====================');
  const xtermCss = await readFile(path.join(root, 'node_modules/@xterm/xterm/css/xterm.css'), 'utf8');
  const app = await bundleWeb4App(root, manifest);
  const html = web4HtmlTemplate(app, bridgeBins, xtermCss);
  await writeFile(path.join(root, 'index.html'), html);
  // keep copy in docs/ for the GitHub pages deploy
  await writeFile(path.join(repo, 'docs', 'index.html'), html);


  console.log(`\nwrote web4/index.html (${html.length} bytes, ` + // eslint-disable-line no-console
    `bridge bins: ${bridgeBins.map((b) => b.name).join(', ')})`);
};

main().catch((e) => { console.error('\nbuild failed:', e.message); process.exit(1); }); // eslint-disable-line no-console
