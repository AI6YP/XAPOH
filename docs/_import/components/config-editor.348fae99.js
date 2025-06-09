import {javascript} from "../../_npm/@codemirror/lang-javascript@6.2.4/a5496f81.js";
import {EditorView, keymap} from "../../_npm/@codemirror/view@6.37.1/684337d2.js";
import {basicSetup} from "../../_npm/codemirror@6.0.1/91a94040.js";
import {oneDark} from "../../_npm/@codemirror/theme-one-dark@6.1.2/58e50fbc.js";

export function configEditor ({
  value = '{ SSID: \'eu2aa\', password: \'eu2aa\' }\n',
  style = 'background: #282c34; width: 100%; min-height: 100px; float: left;'
} = {}) {
  const [outer, parent] = ['div', 'div'].map((t) =>
    document.createElement(t));

  parent.style = style;

  const run = async () => {
    const srcTxt = String(editor.state.doc);
    const src = eval('(() => (' + srcTxt + '))()');
    console.log(src);
  };

  const editor = new EditorView({
    parent,
    doc: value,
    extensions: [
      basicSetup,
      javascript(),
      oneDark,
      keymap.of([
        {key: "Shift-Enter", preventDefault: true, run},
        {key: "Mod-s", preventDefault: true, run}
      ])
    ]
  });

  parent.addEventListener("input", (event) =>
    event.isTrusted && event.stopImmediatePropagation());

    outer.appendChild(parent);
  run();

  return outer;
}