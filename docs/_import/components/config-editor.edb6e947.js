import {javascript} from "../../_npm/@codemirror/lang-javascript@6.2.4/b4353650.js";
import {EditorView, keymap} from "../../_npm/@codemirror/view@6.37.2/9c172c27.js";
import {button} from "../../_observablehq/stdlib/inputs.6dd24f34.js";
import {basicSetup} from "../../_npm/codemirror@6.0.1/704c83ba.js";
import {oneDark} from "../../_npm/@codemirror/theme-one-dark@6.1.2/c93f721a.js";

export function configEditor (obj) {

  let cfgStr = localStorage.getItem('config');
  if (!cfgStr) {
    if (obj?.cfgInit) {
      cfgStr = obj?.cfgInit;
    } else {
      cfgStr = '({ ssid: \'my_ssid\', password: \'my_pass\', eraseAll: false })';
    }
    localStorage.setItem('config', cfgStr);
  }
  
  const style = obj?.style || 'background: #282c34; width: 100%; min-height: 60px; float: left;';

  const [outer, parent] = ['div', 'div'].map((t) =>
    document.createElement(t));

  parent.style = style;

  const run = async () => {
    const cfgTxt = String(editor.state.doc);
    const cfg = eval('(() => (' + cfgTxt + '))()');
    localStorage.setItem('config', cfgTxt);
    outer.value = cfg;
    console.log(cfg);
  };

  const editor = new EditorView({
    parent,
    doc: cfgStr,
    extensions: [
      basicSetup,
      javascript(),
      oneDark,
      keymap.of([
        {key: 'Shift-Enter', preventDefault: true, run},
        {key: 'Mod-s', preventDefault: true, run}
      ])
    ]
  });

  parent.addEventListener('input', (event) =>
    event.isTrusted && event.stopImmediatePropagation());

  const runBtn = button([['SAVE', run]]);
  parent.appendChild(runBtn);
  outer.appendChild(parent);
  run();

  return outer;
}

/* eslint-env browser */
