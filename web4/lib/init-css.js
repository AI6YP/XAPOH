export const initCSS = () => {
  // add document style
  const style = document.createElement('style');
  style.innerHTML = `
    *, *::before, *::after {
      box-sizing: border-box;
    }
    html, body {
      height: 100%;
    }
    body {
      margin: 0;
      font-family: sans-serif;
      background: #111;
      color: #eee;
    }
    #root {
      display: flex;
      flex-direction: column;
      height: 100%;
    }
    .header {
      background: #222;
      border-bottom: 1px solid #444;
      flex: 0 0 auto;
    }
    .header-inner {
      max-width: 1600px;
      margin: 0 auto;
      padding: 0 20px;
      display: flex;
      align-items: center;
      gap: 30px;
    }
    .header-title {
      font-size: 18px;
      font-weight: bold;
      color: #fff;
      padding: 15px 0;
    }
    .content {
      padding: 20px;
      max-width: 1600px;
      margin: 0 auto;
      width: 100%;
      flex: 1 1 auto;
      display: flex;
      flex-direction: column;
      min-height: 0;
    }
    .button {
      margin: 10px 0px;
      padding: 8px 14px;
      background: linear-gradient(180deg, #2a6df4 0%, #1453d6 48%, #0b3fa8 52%, #08369a 100%);
      cursor: pointer;
      border: 1px solid #052a78;
      border-radius: 4px;
      display: inline-block;
      align-self: flex-start;
      color: #fff;
      text-shadow: 0 1px 1px rgba(0, 0, 0, .5);
      box-shadow:
        inset 0 1px 0 rgba(255, 255, 255, .35),
        inset 0 -1px 0 rgba(0, 0, 0, .35),
        1px 1px 0 rgba(0, 0, 0, .45);
      transition: background 0.08s, box-shadow 0.08s, transform 0.08s;
      user-select: none;
    }
    .button:hover {
      background: linear-gradient(180deg, #3a7dff 0%, #1a5ee0 48%, #1149b4 52%, #0e42a6 100%);
    }
    .button:active {
      background: linear-gradient(180deg, #08369a 0%, #0b3fa8 48%, #1453d6 52%, #2a6df4 100%);
      box-shadow:
        inset 0 2px 4px rgba(0, 0, 0, .55),
        inset 0 -1px 0 rgba(255, 255, 255, .12);
      transform: translateY(1px);
    }
    .button:focus-visible {
      outline: 2px solid #f80;
      outline-offset: 1px;
    }
    .button[aria-disabled="true"] {
      opacity: .45;
      cursor: not-allowed;
      filter: grayscale(.5);
      box-shadow:
        inset 0 1px 0 rgba(255, 255, 255, .2),
        inset 0 -1px 0 rgba(0, 0, 0, .25);
      transform: none;
    }
    .button[aria-disabled="true"]:hover,
    .button[aria-disabled="true"]:active {
      background: linear-gradient(180deg, #2a6df4 0%, #1453d6 48%, #0b3fa8 52%, #08369a 100%);
      transform: none;
    }
    .flash-row {
      display: flex;
      gap: 10px;
      margin: 10px 0;
    }
    .flash-row .button {
      flex: 1 1 25%;
      margin: 0;
      text-align: center;
    }
    .success {
      padding: 10px;
      margin: 10px 0px;
      border-radius: 1px;
      background: #040;
      color: #fff;
    }
    .error {
      padding: 10px;
      margin: 10px 0px;
      border-radius: 1px;
      background: #700;
      color: #fff;
    }
    .clear-console {
      padding: 2px 10px;
      font-size: 12px;
      margin-left: 10px;
      vertical-align: middle;
    }
    .cfg-status {
      display: none;
      color: #f88;
      font-size: 12px;
      margin-top: 4px;
    }
    .cfg-reset {
      padding: 4px 10px;
      font-size: 12px;
      margin-top: 6px;
    }
    #bridge-progress {
      margin: 10px 0;
    }
    #bridge-progress .progress-label {
      font-size: 13px;
      margin-bottom: 4px;
    }
    #bridge-progress progress {
      width: 100%;
      height: 16px;
      accent-color: #f80;
    }
    #console {
      flex: 1 1 auto;
      min-height: 300px;
      overflow: hidden;
      display: flex;
      flex-direction: column;
    }
    #console .xterm-host {
      flex: 1 1 auto;
      min-height: 0;
      overflow: hidden;
    }
    #console .xterm-host .xterm,
    #console .xterm-host .xterm .xterm-screen,
    #console .xterm-host .xterm .xterm-scrollable-element {
      height: 100%;
    }
  `;
  document.head.appendChild(style);
};
/* eslint-env browser */
