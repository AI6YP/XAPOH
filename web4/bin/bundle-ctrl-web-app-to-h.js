import { readFile, writeFile } from 'fs/promises';
import path from 'path';

import * as esbuild from 'esbuild';

const indexHtmlTemplate = (script, css) => /*html*/`\
<!doctype html>
<html lang="en">
  <head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1" />
    <link rel="icon" href="data:image/svg+xml,<svg xmlns=%22http://www.w3.org/2000/svg%22 viewBox=%220 0 100 100%22><text y=%22.9em%22 fill=%22grey%22 font-size=%2290%22>📡</text></svg>">
    <title>XAPOH - пульт</title>
    <style>${css}</style>
    <script>${script}</script>
  </head>
  <body><div id="root"></div></body>
</html>
`;

export const bundleCtrlWebAppToH = async (swPanelV4, manifest) => {
  const result = await esbuild.build({
    entryPoints: [path.resolve(swPanelV4, 'lib', 'main.js')],
    bundle: true,
    minify: true,
    write: false,
    logLevel: 'info',
    define: {
      MANIFEST: JSON.stringify(manifest)
    }
  });
  const scriptString = result.outputFiles[0].text;

  const cssString = await readFile(
    path.resolve(swPanelV4, 'lib', 'main.css'),
    {encoding: 'utf8'}
  );
  const htmlText = indexHtmlTemplate(scriptString, cssString);
  // await writeFile(path.resolve(swPanelV4, 'lib', 'index.html'), htmlText);
  const items = [{
    name: 'index',
    ext: '.html',
    data: Buffer.from(htmlText, 'utf8')
  }];
  const res = headerTemplate(items);
  await writeFile(path.resolve(swPanelV4, 'main', 'pages.h'), res);
  console.log('pages.h written (%d bytes html)', htmlText.length);
};

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


