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
    .tabs {
      display: flex;
      gap: 5px;
    }
    .tab {
      padding: 15px 20px;
      cursor: pointer;
      color: #888;
      border-bottom: 2px solid transparent;
      transition: color 0.2s, border-color 0.2s;
    }
    .tab:hover {
      color: #ccc;
    }
    .tab.active {
      color: #fff;
      border-bottom-color: #f80;
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
    .tab-panel {
      display: none;
    }
    .tab-panel.active {
      display: block;
    }
    .button {
      margin: 10px 0px;
      padding: 10px;
      background: #047;
      cursor: pointer;
      border-radius: 10px;
      display: inline-block;
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
