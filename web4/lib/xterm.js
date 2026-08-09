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
  div.className = 'xterm-host';
  term.open(div);
  const fit = () => {
    // FitAddon.fit() no-ops if the terminal has no measured parent, but guard
    // anyway so callers (ResizeObserver, window resize) never throw before the
    // terminal is attached to the document.
    if (!term.element || !term.element.parentElement) return;
    fitAddon.fit();
  };
  window.addEventListener('resize', fit);
  return {
    div,
    term,
    fit,
    callbacks: {
      clean: () => {
        fit();
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
