#!/usr/bin/env node
'use strict';

const { readFile, writeFile } = require('fs/promises');
const path = require('path');
const browserify = require('browserify');

const indexHtmlTemplate = (script, css) => `\
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1" />
<link rel="icon" href="data:image/svg+xml,<svg xmlns=%22http://www.w3.org/2000/svg%22 viewBox=%220 0 100 100%22><text y=%22.9em%22 fill=%22grey%22 font-size=%2290%22>📡</text></svg>">
<title>XAPOH</title>
<script>${script}</script>
<style>${css}</style>
</head>
<body onload="XAPOH('root')">
<body><div id="root"></div></body>
</html>
`;

const headerTamplate = (items) => `
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
  const bro = browserify();
  bro.add(path.resolve(__dirname, 'lib', 'main.js'));
  const scriptString = await new Promise((resolve, reject) => {
    bro.bundle((err, res) => {
      if (err) { reject(err); } else { resolve(res); }
    });
  });

  const cssString = await readFile(
    path.resolve(__dirname, 'lib', 'main.css'),
    {encoding: 'utf8'}
  );
  const htmlText = indexHtmlTemplate(scriptString, cssString);
  await writeFile(path.resolve(__dirname, 'lib', 'index.html'), htmlText);
  const items = [{
    name: 'index',
    ext: '.html',
    data: Buffer.from(htmlText, 'utf8')
  }];
  // console.log(items);
  const res = headerTamplate(items);
  // // console.log(res);
  await writeFile(path.resolve(__dirname, 'main', 'pages.h'), res);
};

main();
