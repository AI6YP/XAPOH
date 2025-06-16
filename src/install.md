---
title: Install
style: style.css
toc: false
---

```js
import {Mutable} from "observablehq:stdlib";
import {bins} from './components/bins.js';
import {configEditor} from './components/config-editor.js';
import {onEspConnectClick, onResetClick, onProgramClick, onProgressBar, xterm} from './components/esp-install.js';

const xterm0 = xterm();
```

# Install software

```js
const esp = view(Inputs.button(
  html`Connect to board`,
  {value: null, reduce: () => onEspConnectClick(xterm0)}
));
```

```js
const resetButton = view(Inputs.button('Reset', {
  disabled: (esp === null),
  value: null, reduce: () => onResetClick(esp)
}));
```

```js
const cfg = view(configEditor());
```

```js
const programButton = view(Inputs.button('Program Firmware: ' + bins[0].time, {
  disabled: (esp === null),
  value: null,
  reduce: () =>
    onProgramClick(esp, cfg)
}));
```

```js
const progressBar = Generators.observe(onProgressBar);
```

${progressBar.fileIndex} : ${Math.round(100 * progressBar.written / progressBar.total)}%


```js
xterm0.div
```
