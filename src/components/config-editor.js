import {javascript} from "npm:@codemirror/lang-javascript";
import {EditorView, keymap} from "npm:@codemirror/view";
import {basicSetup} from "npm:codemirror";
import {oneDark} from 'npm:@codemirror/theme-one-dark';

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