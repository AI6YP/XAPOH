'use strict';

import { md5 } from 'npm:js-md5';
import { ESPLoader, Transport } from 'npm:esptool-js';
import { bins } from './bins.js';
import { t2pt } from './t2pt.js';
import { Terminal } from 'npm:@xterm/xterm';
import { FitAddon } from 'npm:@xterm/addon-fit';
import { ClipboardAddon } from 'npm:@xterm/addon-clipboard';

const ui8ToBstr = (t) => {
  let e = "";
  for (let s = 0; s < t.length; s++)
      e += String.fromCharCode(t[s]);
  return e;
};

const state = {
  change: () => {}
};

export async function onEspConnectClick (xterm) {

  // esp32c6
  const filters = [{usbVendorId: 0x303a, usbProductId: 0x1001}];

  const port = await navigator.serial.requestPort({filters});
  const baudrate = [115200, 460800, 921600][1];
  // await port.open({baudRate});
  const transport = new Transport(port, true);
  const flashOptions = {transport, baudrate, terminal: xterm.callbacks};
  // debugLogging: debugLogging.checked,

  const esploader = new ESPLoader(flashOptions);
  const chip = await esploader.main();
  const progbar = (val) => {
    console.log(val)
  };
  const ret = {port, transport, chip, esploader, progbar, xterm, terminal: xterm.callbacks};
  console.log(ret)
  return ret;
}

export async function onResetClick (esp) {
  const { transport, xterm } = esp;
  if (transport) {
    await transport.setDTR(false);
    await new Promise((resolve) => setTimeout(resolve, 100));
    await transport.setDTR(true);

    const writer = transport.device.writable.getWriter();

    xterm.term.onData((data) => {
      writer.write(new TextEncoder().encode(data))
    });

    while (true) {
      const readLoop = transport.rawRead();
      const { value, done } = await readLoop.next();
      if (done || !value) {
        break;
      }
      xterm.term.write(value);
    }
  }
}

// erase button?
// disconnectButton
// consoleStartButton

const partTable = [ // ESP-IDF Partition Table
  // name: partition_table, offset: 0x8000, size: 0x1000 (1 flash sector)
  //      v--------------v
  {name: 'nvs',       type: 'data.nvs',     offset: 0x9000,   size: 0x6000},
  {name: 'phy_init',  type: 'data.phy',     offset: 0xf000,   size: 0x1000},
  {name: 'factory',   type: 'app.factory',  offset: 0x10000,  size: 0x100000}
];


export async function onProgramClick (esp, progressBar) {
  console.log(progressBar);
  const { esploader, transport, terminal } = esp;
  const fileArray = [
    {
      address: 0x8000,
      data: ui8ToBstr(await t2pt(partTable, (val) => new Uint8Array(md5.arrayBuffer(val))
      ))
    },
    ...bins.map(e => ({
      address: e.address,
      data: atob(e.data)
    }))
  ];
  const flashOptions = {
    fileArray: fileArray,
    flashSize: 'keep',
    eraseAll: false,
    compress: true,
    reportProgress: (fileIndex, written, total) => {
      console.log({fileIndex, written, total});
      state.change({fileIndex, written, total});
    },
    // calculateMD5Hash: md5
      // CryptoJS.MD5(CryptoJS.enc.Latin1.parse(image))
  };
  await esploader.writeFlash(flashOptions);
  await esploader.after();
  console.log('done programming');

  while (true) {
    const readLoop = transport.rawRead();
    const { value, done } = await readLoop.next();
    if (done || !value) {
      break;
    }
    terminal.write(value);
  }
}

export function onProgressBar (change) {
  state.change = change;
  return () => { state.change = () => {}; }
};

export const xterm = () => {
  const term = new Terminal({
    rows: 30,
    cols: 120,
    cursorBlink: true,
    cursorStyle: 'block',
    fontFamily: 'Iosevka Drom Web'
  });
  const fitAddon = new FitAddon();
  term.loadAddon(fitAddon);
  const clipboardAddon = new ClipboardAddon();
  term.loadAddon(clipboardAddon);
  const div = document.createElement('div');
  term.open(div);
  window.addEventListener('resize', () => {
    fitAddon.fit();
  });
  return {
    div,
    term,
    callbacks: {
      clean: () => {
        fitAddon.fit();
        term.clear();
      },
      writeLine: (data) => {
        term.writeln(data);
      },
      write: (data) =>{
        term.write(data);
      }
    }
  };
};
