#!/usr/bin/env node

import path from 'path';
import childProcess from 'child_process';
import { Command } from 'commander';
import chokidar from 'chokidar';

const run = (opts) => new Promise((done) => {
  // protect from ETXTBSY
  setTimeout(() => {
    try {
      const child = childProcess.spawn(opts.run,
        (opts.verbose ? ['-v'] : []),
        { stdio: 'inherit' } // This makes stdout and stderr transparent
      );

      child.on('exit', (code) => {
        console.log(`Child process exited with code ${code}`); // eslint-disable-line no-console
        done();
      });
    } catch (e) {
      console.error(e); // eslint-disable-line no-console
      done();
    }
  }, 100);
});

const watch = async () => {
  const program = new Command();

  program
    .requiredOption('-r, --run <executable>', 'run executable after build')
    .option('-v, --verbose', 'verbosity that can be increased', (_, v) => v + 1, 0)
    .argument('<paths...>', 'paths to watch for changes')
    .parse(process.argv);

  const opts = program.opts();

  console.log(opts); // eslint-disable-line no-console
  const fullWatchPoints = program.args
    // .concat ['./src/', './include/', './lib/', './build/']
    .map(p => path.resolve(p));

  await run(opts);

  const state = {running: false, pending: false};

  const watcher = chokidar.watch(fullWatchPoints, {
    ignored: /(^|[/\\])\../, // ignore dotfiles
    persistent: true
  });

  watcher.on('change', (filename) => {
    if (opts.verbose) {
      console.log(filename + ' : changed'); // eslint-disable-line no-console
    }
    if (state.running) {
      state.pending = true;
      return;
    }
    state.running = true;
    state.pending = false;
    run(opts).then(() => {
      state.running = false;
      if (state.pending) {
        run(opts);
        state.pending = false;
      }
    });
  });
};

watch();
