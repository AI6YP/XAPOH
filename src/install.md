---
title: Install
style: style.css
toc: false
---

```js
import {Mutable} from "observablehq:stdlib";
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
const programButton = view(Inputs.button('Program Firmware v2025.06.08', {
  disabled: (esp === null),
  value: null,
  reduce: () =>
    onProgramClick(esp)
}));
```

```js
const progressBar = Generators.observe(onProgressBar);
```

${progressBar.fileIndex} : ${Math.round(100 * progressBar.written / progressBar.total)}%


```js
xterm0.div
```
