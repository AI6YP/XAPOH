#!/usr/bin/env node
'use strict';

const fs = require('fs');
const path = require('path');

const { readFile, writeFile } = fs.promises;

const prjPath = path.resolve(__dirname, '../sw-panel-v3/build');

const files = [
  // {addr: 0x8000,  name: 'partition_table/partition-table.bin'},
  {addr: 0x0,     name: 'bootloader/bootloader.bin'},
  {addr: 0x10000, name: 'sw-panel-v3.bin'},
];

const main = async () => {
  let res = `\
export const bins = [`;
  for (const file of files) {
    const baseName = path.basename(file.name);
    const fullPath = path.resolve(prjPath, file.name);
    const rawData = await readFile(fullPath);
    console.log({fullPath, length: rawData.length / 1024 / 1024});
    const data = await readFile(fullPath, 'base64');
    res += `
{
  name: "${baseName}",
  address: 0x${file.addr.toString(16)},
  time: "${(new Date().toISOString())}",
  data: "${data}"
},`;
  }
  res += `
];
`;
  const outFilePath = path.resolve(__dirname, '../src/components/bins.js');
  await writeFile(outFilePath, res, 'utf8');
};

main();
