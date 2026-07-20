import { Terminal } from '@xterm/xterm';
import { FitAddon } from '@xterm/addon-fit';
import { ClipboardAddon } from '@xterm/addon-clipboard';

export const xterm = () => {
  const term = new Terminal({
    rows: 30,
    cols: 120,
    cursorBlink: true,
    cursorStyle: 'block',
    fontFamily: '"Iosevka Drom Web", monospace'
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
    fit: () => fitAddon.fit(),
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

/* eslint-env browser */
