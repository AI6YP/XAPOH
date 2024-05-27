#!/usr/bin/env node
'use strict';

const process = require('process');
const path = require('path');
const fs = require('fs');

const commander = require('commander');
const stlSerializer = require('@jscad/stl-serializer');

commander
  .requiredOption('-m, --model <path>', 'input OpenJsCad model to render')
  .option('-o, --output-stl <path>', 'output STL file to save')
  .parse(commander.argv);

const options = commander.opts();

const model = require(path.join(process.cwd(), options.model));

const raw = stlSerializer.serialize({binary: true}, model.main());

const stl = Buffer.concat([Buffer.from(raw[0]), Buffer.from(raw[1]), Buffer.from(raw[2])]);

if (options.outputStl) {
  fs.writeFileSync(options.outputStl, stl);
} else {
  process.stdout.write(stl);
}
