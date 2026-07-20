import path from 'path';
import * as esbuild from 'esbuild';

export const web4HtmlTemplate = (app, bridgeBins, xtermCss) => /*html*/`\
<!doctype html>
<html lang="en">
  <head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1" />
    <link rel="icon" href="data:image/svg+xml,<svg xmlns=%22http://www.w3.org/2000/svg%22 viewBox=%220 0 100 100%22><text y=%22.9em%22 fill=%22grey%22 font-size=%2290%22>📡</text></svg>">
    <title>XAPOH - программатор</title>
    <style>${xtermCss}</style>
    <script>window.BRIDGE_BINS = ${JSON.stringify(bridgeBins)};</script>
    <script>${app}</script>
  </head>
  <body><div id="root"></div></body>
</html>
`;

export const bundleWeb4App = async (web4path, manifest) => {
  const res = await esbuild.build({
    entryPoints: [path.join(web4path, 'lib', 'main.js')],
    bundle: true,
    minify: false,
    write: false,
    format: 'esm',
    platform: 'browser',
    define: {
      MANIFEST: JSON.stringify(manifest)
    }
  });
  return res.outputFiles[0].text;
};

