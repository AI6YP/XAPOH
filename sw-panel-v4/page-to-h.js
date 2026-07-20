#!/usr/bin/env node
'use strict';

// Hosted-UI build: bundle lib/main.js with esbuild (was browserify in v3),
// wrap in the index template, emit main/pages.h (PAGE_index byte array) that the
// ESP serves at '/'.

const fs = require('fs');
const path = require('path');
const esbuild = require('esbuild');
const manifest = require('../manifest.json');

const indexHtmlTemplate = (script, css) => `\
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1" />
<link rel="icon" href="data:image/svg+xml,<svg xmlns=%22http://www.w3.org/2000/svg%22 viewBox=%220 0 100 100%22><text y=%22.9em%22 fill=%22grey%22 font-size=%2290%22>📡</text></svg>">
<title>XAPOH</title>
<script>document.XAPOH_VERSION = '${manifest.version}';</script>
<script>${script}</script>
<style>${css}</style>
</head>
<body onload="XAPOH('root')">
<body><div id="root"></div></body>
</html>
`;

const headerTemplate = (items) => `
/* Binary array for the Web UI. */
${items.map((item) => `
// Generated!! do not edit!!
const uint16_t PAGE_${item.name}_length = ${item.data.length};
DRAM_ATTR const char PAGE_${item.name}[] = {\
${(() => {
    let res = '';
    for (let idx = 0; idx < item.data.length; idx++) {
      const c = (typeof item.data === 'string') ? item.data.charCodeAt(idx) : item.data[idx];
      res += (idx & 15) ? ' ' : '\n  ';
      res += '0x' + c.toString(16).padStart(2, 0);
      if (idx < (item.data.length - 1)) {
        res += ',';
      }
    }
    return res;
  })()}
};
`).join(', ')}
`;

const main = async () => {
  const result = await esbuild.build({
    entryPoints: [path.resolve(__dirname, 'lib', 'main.js')],
    bundle: true,
    minify: true,
    write: false,
    logLevel: 'info'
  });
  const scriptString = result.outputFiles[0].text;

  const cssString = await fs.promises.readFile(
    path.resolve(__dirname, 'lib', 'main.css'),
    {encoding: 'utf8'}
  );
  const htmlText = indexHtmlTemplate(scriptString, cssString);
  await fs.promises.writeFile(path.resolve(__dirname, 'lib', 'index.html'), htmlText);
  const items = [{
    name: 'index',
    ext: '.html',
    data: Buffer.from(htmlText, 'utf8')
  }];
  const res = headerTemplate(items);
  await fs.promises.writeFile(path.resolve(__dirname, 'main', 'pages.h'), res);
  console.log('pages.h written (%d bytes html)', htmlText.length);
};

main();
