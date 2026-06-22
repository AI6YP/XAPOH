import { ESPLoader, Transport, CustomReset } from 'esptool-js';
import { md5 } from 'js-md5';
import { wchlink } from './wchlink.js';
import { xterm } from './xterm.js';
import { partTable, t2pt, cfg2ui8 } from './esp-partition.js';
import { mountBridgeConfigEditor } from './mount-bridge-config-editor.js';
import { initCSS } from './init-css.js';

// const pkg = require('../package.json');
// import pkg from '../package.json' assert { type: 'json' };
const pkgVersion = 'v9 2026-06-22 00:00';

// esptool-js 0.6 wants each fileArray entry's `data` as a Uint8Array.
const b64ToUi8 = (b64) => Uint8Array.from(atob(b64), (c) => c.charCodeAt(0));

// Build the flash payload: partition table @0x8000 (built here), the embedded
// firmware bins (bootloader @0x0, app @0x10000), and the wifi config @0x110000.
const buildFileArray = async (cfg) => [
  {
    address: 0x8000,
    data: await t2pt(partTable, (val) => new Uint8Array(md5.arrayBuffer(val)))
  },
  ...window.BRIDGE_BINS.map((e) => ({address: e.address, data: b64ToUi8(e.data)})),
  {
    address: partTable.find((e) => e.name === 'config').offset,
    data: cfg2ui8(cfg)
  }
];

const genOnClickActivateBridge = () => async () => {
  const filters = [{usbVendorId: 0x303a, usbProductId: 0x1001}]; // ESP32-C6
  const port = await navigator.serial.requestPort({filters});
  const baudrate = [115200, 460800, 921600][1];
  const transport = new Transport(port, true);
  const term = xterm();
  const flashOptions = {transport, baudrate, terminal: term.callbacks};
  const esploader = new ESPLoader(flashOptions);
  const chip = await esploader.main();
  console.log(chip); // eslint-disable-line no-console

  const tab = document.getElementById('panel-bridge');
  tab.innerHTML = `
    <div class="success">Мост подключен</div>
    <div>
      Информация о чипе: <pre>${JSON.stringify(chip, null, 2)}</pre>
    </div>
    <h3>Конфигурация</h3>
    <div class="bridge-config" id="bridgeConfig"></div>
    <div class="button" id="buttonBridgeFlash">Прошить bridge-fw-${pkgVersion}</div>
    <div id="bridge-progress"></div>
    <p>Консоль:</p>
    <div id="console"></div>
  `;
  mountBridgeConfigEditor(tab.querySelector('#bridgeConfig'));

  const progress = tab.querySelector('#bridge-progress');
  tab.querySelector('#buttonBridgeFlash').onclick = async () => {
    let cfg;
    try {
      cfg = JSON.parse(localStorage.getItem('bridgeConfig') || '{}');
    } catch (e) {
      progress.textContent = 'ОШИБКА: неверный JSON конфигурации';
      return;
    }

    const fileArray = await buildFileArray(cfg);
    await esploader.writeFlash({
      fileArray,
      flashSize: 'keep',
      eraseAll: false,
      compress: true,
      reportProgress: (fileIndex, written, total) => {
        progress.textContent = `Прошивка ${fileIndex + 1}/${fileArray.length}: ${written}/${total}`;
      }
    });
    progress.textContent = 'Прошивка завершена. Перезагрузка…';

    // ESP32-C6 talks over the native USB-Serial-JTAG. The reset-to-run-app
    // sequence (esptool.py HardReset, uses_usb=True) pulses EN via RTS while
    // leaving the boot strap (DTR/IO0) released: RTS=1 (EN low) → wait → RTS=0.
    // esploader.after()'s HardReset omits the RTS=1 pulse (no-op), and
    // UsbJtagSerialReset is the enter-bootloader sequence — neither runs the app.
    // The pulse resets the USB-Serial-JTAG too, so the port drops mid-reset.
    try { await new CustomReset(transport, 'R1|W200|R0|W200').reset(); } catch (e) { /* port drops mid-reset */ }

    // The reset re-enumerates the USB-Serial-JTAG device, which usually
    // invalidates this port handle. Reconnect at the firmware console baud
    // (flash baud != console baud); if the handle is gone, ask the user.
    try {
      await transport.disconnect();
      await transport.connect(115200); // CONFIG_ESP_CONSOLE_UART_BAUDRATE

      // esptool-js 0.6: rawRead(onData, isClosed) is callback-based, not a generator.
      tab.querySelector('#console').appendChild(term.div);
      term.fit(); // size to the container now that it's in the DOM
      const writer = transport.device.writable.getWriter();
      term.term.onData((data) => writer.write(new TextEncoder().encode(data)));
      await transport.rawRead(
        (value) => term.term.write(value),
        () => false
      );
    } catch (e) {
      progress.textContent = 'Готово. Переподключите USB для консоли.';
    }
  };
};

const genOnClickActivateExpander = () => async () => {
  const filters = [{vendorId: 0x1a86, productId: 0x8010}]; // WCH-LinkE
  const tab = document.getElementById('panel-expander');

  let device;
  try {
    device = await navigator.usb.requestDevice({filters});
  } catch (e) {
    return; // user cancelled the chooser
  }

  // render the panel up front so progress is visible during the flash
  tab.innerHTML = `
    <div class="success">Экспандер подключен</div>
    <div id="expander-info"></div>
    <p>Консоль:</p>
    <pre id="expander-log" style="background:#030;color:#fff;border-radius:10px;padding:10px;height:240px;overflow:auto;white-space:pre-wrap;"></pre>
    <div class="button" id="buttonExpander">Повторить</div>
  `;
  document.getElementById('buttonExpander').onclick = genOnClickActivateExpander();

  const log = (msg) => {
    const el = tab.querySelector('#expander-log');
    el.textContent += msg + '\n';
    el.scrollTop = el.scrollHeight;
    console.log(msg); // eslint-disable-line no-console
  };

  // wchlink reports progress via onProgress({phase, done, total})
  const onProgress = ({phase, done, total}) => {
    log(total ? `${phase}: ${done}/${total}` : phase);
  };

  let wchlink0;
  try {
    log('Подключение к программатору…');
    wchlink0 = await wchlink({device, onProgress});

    const wchlinkInfo = await wchlink0.programmerInfo(); // WCH-LinkE info
    log(`Программатор: ${wchlinkInfo.name} v${wchlinkInfo.version}`);

    const mcuChipInfo = await wchlink0.chipInfo(); // CH32V003 chip info
    log(`Чип: ${mcuChipInfo.name}, UUID ${mcuChipInfo.uuid}`);
    tab.querySelector('#expander-info').innerHTML =
      `<pre>${JSON.stringify({wchlink: wchlinkInfo, mcu: mcuChipInfo}, null, 2)}</pre>`;

    const expanderImage = Uint8Array.from(atob(window.EXPANDER_IMAGE), (c) => c.charCodeAt(0));
    log(`Образ: ${expanderImage.length} байт`);

    await wchlink0.nrstAsGpio();   // NRST -> GPIO (also unlocks + erases flash)
    await wchlink0.writeImage(expanderImage);
    log('Прошивка завершена.');
  } catch (e) {
    log('ОШИБКА: ' + e.message);
  } finally {
    if (wchlink0) await wchlink0.close();
  }
};

const initHtmlBridge = (hasSerial) => hasSerial ? `
  <p>Раздел для эксплуатации WiFI-Моста (ESP32-C6):</p>
  <ul>
    <li>прошивка программы</li>
    <li>конфигурация WiFi</li>
    <li>консоль</li>
  </ul>
  <p>Для начала работы, подключите Мост к USB и нажмите кнопку:</p>
  <div class="button" id="buttonBridge">Активировать Мост</div>
` : `
  <p>Ваш браузер:</p>
  <p><code>${navigator.userAgent}</code></p>
  <p>не поддерживает WebSerial API</p>
  <p>Попробуйте другой браузер, например Chrome</p>
`;

const initHtmlExpander = (hasUsb) => hasUsb ? `
  <p>Раздел для программирования I2C Экспандера (CH32V003):</p>
  <p>Для начала работы:
  <ul>
    <li>подключите Экспандер к WCH-Link</li>
    <li>подключите WCH-Link к USB</li>
    <li>нажмите кнопку:</li>
  </ul>
  <div class="button" id="buttonExpander">Активировать Экспандер</div>
` : `
  <p>Ваш браузер:</p>
  <p><code>${navigator.userAgent}</code></p>
  <p>не поддерживает WebUSB API</p>
  <p>Попробуйте другой браузер, например Chrome</p>
`;

const initHtml = ($root, genOnClickActivateBridge, genOnClickActivateExpander) => {
  $root.innerHTML = `
    <div class="header">
      <div class="header-inner">
        <div class="header-title">XAPOH ${pkgVersion}</div>
        <div class="tabs">
          <div class="tab active" data-tab="bridge">Мост</div>
          <div class="tab" data-tab="expander">Экспандер</div>
        </div>
      </div>
    </div>
    <div class="content">
      <div class="tab-panel active" id="panel-bridge">
        ${initHtmlBridge(genOnClickActivateBridge)}
      </div>
      <div class="tab-panel" id="panel-expander">
        ${initHtmlExpander(genOnClickActivateExpander)}
      </div>
    </div>
  `;

  // Setup button handlers
  if (genOnClickActivateBridge) {
    document.getElementById('buttonBridge').onclick = genOnClickActivateBridge();
  }
  if (genOnClickActivateExpander) {
    document.getElementById('buttonExpander').onclick = genOnClickActivateExpander();
  }

  // Tab switching
  const tabs = $root.querySelectorAll('.tab');
  const panels = $root.querySelectorAll('.tab-panel');

  tabs.forEach(tab => {
    tab.addEventListener('click', () => {
      const tabName = tab.dataset.tab;

      // Update tab styles
      tabs.forEach(t => t.classList.remove('active'));
      tab.classList.add('active');

      // Show/hide panels
      panels.forEach(panel => {
        panel.classList.toggle('active', panel.id === `panel-${tabName}`);
      });
    });
  });
};

const onLoad = async () => {
  console.log(pkgVersion); // eslint-disable-line no-console
  initCSS();
  initHtml(
    document.getElementById('root'),
    ('serial' in navigator) && genOnClickActivateBridge,
    ('usb' in navigator) && genOnClickActivateExpander
  );
};

document.addEventListener('DOMContentLoaded', onLoad);
/* eslint-env browser */
