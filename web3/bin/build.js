#!/usr/bin/env node

// bin/build.js
import * as esbuild from 'esbuild';
import { readFile, writeFile } from 'fs/promises';
import path from 'path';

// Build CH32V003 Expander code base64 string
const getExpanderImage = async (binFileName) => {
  const bin = await readFile(binFileName);
  const base64 = bin.toString('base64');
  return base64;
};

// Embed ESP32-C6 bridge firmware partitions as base64 (bin -> js).
// Paths/addresses match sw-panel-v3/build/flasher_args.json (the partition
// table @0x8000 is built at runtime in the browser, so it is not embedded).
const bridgeBuildDir = '../sw-panel-v3/build';
const bridgeFiles = [
  {address: 0x0,     file: 'bootloader/bootloader.bin'},
  {address: 0x10000, file: 'sw-panel-v3.bin'}
];

const getBridgeBins = async () => {
  const bins = [];
  for (const {address, file} of bridgeFiles) {
    const full = path.join(bridgeBuildDir, file);
    const data = (await readFile(full)).toString('base64');
    bins.push({name: path.basename(file), address, data});
  }
  return bins;
};

// Build application code
const build = async () => {
  const res = await esbuild.build({
    entryPoints: ['lib/main.js'],
    bundle: true,
    minify: false,
    write: false,
    outfile: 'main.js', // for internal use only
    format: 'esm',  // или 'iife' для браузера
    platform: 'browser'
  });
  const bundledJs = res.outputFiles[0].text;
  return bundledJs;
};

const htmlTemplate = (app, expanderImage, bridgeBins, xtermCss) => `\
<!doctype html>
<html lang="en">
  <head>
    <meta charset="utf-8">
    <link rel="icon" href="data:image/svg+xml,<svg xmlns=%22http://www.w3.org/2000/svg%22 viewBox=%220 0 100 100%22><text y=%22.9em%22 fill=%22grey%22 font-size=%2290%22>🪐</text></svg>">
    <title>XAPOH</title>
    <style>${xtermCss}</style>
    <script>window.EXPANDER_IMAGE = "${expanderImage}";</script>
    <script>window.BRIDGE_BINS = ${JSON.stringify(bridgeBins)};</script>
    <script>${app}</script>
  </head>
  <body>
    <div id="root"></div>
  </body>
</html>
`;

const main = async () => {
  const expanderImage = await getExpanderImage('../sw-v3/expander/main.bin');
  // const expanderImage = await getExpanderImage('./test/blink.bin');
  const bridgeBins = await getBridgeBins();
  // xterm.js needs its stylesheet for correct glyph metrics (monospace layout)
  const xtermCss = await readFile('node_modules/@xterm/xterm/css/xterm.css', 'utf8');
  const bundledJs = await build();
  const html = htmlTemplate(bundledJs, expanderImage, bridgeBins, xtermCss);
  await writeFile('index.html', html);
};

main();
