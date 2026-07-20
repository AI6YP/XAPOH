#!/usr/bin/env node
'use strict';

// Convert sw-v4/main.bin (CH32V003 expander firmware) into a C header the ESP
// app compiles in — replaces the IDF EMBED_FILES symbol. Mirrors page-to-h.js.
// Run before `idf.py build` (web4/bin/build.js does this after `make -C sw-v4`).

const fs = require('fs');
const path = require('path');

const binPath = path.resolve(__dirname, '..', 'sw-v4', 'main.bin');
const outPath = path.resolve(__dirname, 'main', 'expander_image.h');

const toHeader = (data) => {
  let body = '';
  for (let i = 0; i < data.length; i++) {
    body += (i & 15) ? ' ' : '\n  ';
    body += '0x' + data[i].toString(16).padStart(2, '0');
    if (i < data.length - 1) { body += ','; }
  }
  return `\
/* CH32V003 expander firmware image (sw-v4/main.bin). Generated!! do not edit!! */
#pragma once
const unsigned int expander_bin_length = ${data.length};
const unsigned char expander_bin[] = {${body}
};
`;
};

const main = async () => {
  const data = await fs.promises.readFile(binPath);
  await fs.promises.writeFile(outPath, toHeader(data));
  console.log('expander_image.h written (%d bytes)', data.length); // eslint-disable-line no-console
};

main();
