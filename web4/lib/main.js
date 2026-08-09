import { ESPLoader, Transport, CustomReset } from 'esptool-js';
import { md5 } from 'js-md5';
import { xterm } from './xterm.js';
import { partTable, t2pt, cfg2ui8 } from './esp-partition.js';
import { mountBridgeConfigEditor } from './mount-bridge-config-editor.js';
import { initCSS } from './init-css.js';
import { connectEsp, sendCommand } from './esp-serial.js';

// eslint-disable-next-line no-undef
const manifest = MANIFEST;

// esptool-js 0.6 wants each fileArray entry's `data` as a Uint8Array.
const b64ToUi8 = (b64) => Uint8Array.from(atob(b64), (c) => c.charCodeAt(0));

// Build the flash payload. The partition table @0x8000 is built HERE (not the
// idf-built one): the firmware needs a `config` data partition that the default
// singleapp table lacks — see partTable in esp-partition.js. Then the embedded
// bins (bootloader @0x0, app @0x10000) and the wifi config @ config offset.
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

// --- Bridge tab: flash the ESP32-C6 firmware (esptool-js over WebSerial) ------
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

  const tab = document.getElementById('content');
  tab.innerHTML = /*html*/`
    <div class="success">Мост подключен: ${chip}</div>
    <h3>Конфигурация</h3>
    <div class="bridge-config" id="bridgeConfig"></div>
    <div class="flash-row">
      <span class="button flush" id="buttonBridgeFlash">Прошить ESP32C6</span>
      <span class="button flash" id="buttonExpanderFlash0">Прошить D50</span>
      <span class="button flash" id="buttonExpanderFlash1">Прошить D51</span>
      <span class="button flash" id="buttonExpanderFlash2">Прошить D52</span>
    </div>
    <div id="bridge-progress"></div>
    <p>Консоль:</p>
    <div id="console"></div>
  `;
  mountBridgeConfigEditor(tab.querySelector('#bridgeConfig'));

  const progress = tab.querySelector('#bridge-progress');
  let consoleWriter = null; // Store the writer for reuse by expander buttons
  let consoleInitialized = false; // Track if console has been initialized

  // D5x expander buttons stay disabled until the ESP32C6 firmware flash
  // completes and the chip reboots — they talk to the ESP over the post-flash
  // serial console, which only exists after "Прошивка завершена. Перезагрузка…".
  const expanderBtns = [
    tab.querySelector('#buttonExpanderFlash0'),
    tab.querySelector('#buttonExpanderFlash1'),
    tab.querySelector('#buttonExpanderFlash2')
  ];
  const setExpanderEnabled = (on) => expanderBtns.forEach((b) => {
    if (!b) return;
    b.style.opacity = on ? '1' : '.4';
    b.dataset.on = on ? '1' : '';
  });
  setExpanderEnabled(false);

  // Mount xterm terminal immediately (but don't connect to serial yet)
  const consoleEl = tab.querySelector('#console');
  consoleEl.appendChild(term.div);
  // Keep the terminal fitted to its flex container: a ResizeObserver on
  // #console catches every size change — window resize, the config editor
  // growing/shrinking above, and the initial flex layout settling — and
  // re-runs fit() so rows/cols track the box. RO also fires once on attach,
  // covering the first layout.
  const ro = new ResizeObserver(() => term.fit());
  ro.observe(consoleEl);

  // Switch to serial mode and start reading (called when expander button is pressed or after ESP32C6 flash)
  const initConsole = async () => {
    if (consoleInitialized) return; // Already initialized
    try {
      await transport.disconnect();
      await transport.connect(115200); // CONFIG_ESP_CONSOLE_UART_BAUDRATE

      consoleWriter = transport.device.writable.getWriter();
      term.term.onData((data) => consoleWriter.write(new TextEncoder().encode(data)));
      consoleInitialized = true;
      await transport.rawRead(
        (value) => term.term.write(value),
        () => false
      );
    } catch (e) {
      progress.textContent = 'Ошибка инициализации консоли: ' + e.message;
    }
  };

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
    setExpanderEnabled(true);

    // ESP32-C6 talks over the native USB-Serial-JTAG. The reset-to-run-app
    // sequence pulses EN via RTS while leaving the boot strap released. The
    // pulse resets the USB-Serial-JTAG too, so the port drops mid-reset.
    try { await new CustomReset(transport, 'R1|W200|R0|W200').reset(); } catch (e) { /* port drops mid-reset */ }

    // The reset re-enumerates the USB-Serial-JTAG device, usually invalidating
    // this port handle. Reconnect at the firmware console baud; if the handle is
    // gone, ask the user.
    // Note: initConsole() already mounted the terminal, so we just need to
    // reconnect after reset and reinitialize console
    try {
      consoleInitialized = false; // Reset flag to allow re-initialization
      await initConsole(); // This will reconnect and remount console
    } catch (e) {
      progress.textContent = 'Готово. Переподключите USB для консоли.';
    }
  };

  // --- Expander flash buttons: send F0/F1/F2 commands to ESP via serial --------
  const setupExpanderButton = (buttonId, cmd, name) => {
    const btn = tab.querySelector(`#${buttonId}`);
    if (!btn) return;

    btn.onclick = async () => {
      if (!btn.dataset.on) {
        return; // disabled until ESP32C6 flash completes
      }
      console.log(`[Expander] Button clicked: ${name}, cmd: ${cmd}`); // Debug
      progress.textContent = `Прошивка ${name}...`;
      term.term.writeln(`\r\n[web4] Прошивка ${name}, команда: ${cmd}`); // Show in terminal

      try {
        // Initialize console if not already done
        if (!consoleWriter) {
          console.log('[Expander] Initializing console...'); // Debug
          term.term.writeln('[web4] Инициализация консоли...');
          progress.textContent = 'Инициализация консоли...';
          await initConsole();
          // Wait a bit for console to be ready
          await new Promise(resolve => setTimeout(resolve, 500));
        }

        if (!consoleWriter) {
          throw new Error('Console writer not initialized');
        }

        console.log(`[Expander] Sending command: ${cmd}`); // Debug
        term.term.writeln(`[web4] Отправка команды: ${cmd}`);

        // Send flash command using the console writer
        await consoleWriter.write(new TextEncoder().encode(cmd));
        progress.textContent = `Команда ${cmd} отправлена для ${name}.`;
        term.term.writeln(`[web4] Команда отправлена`);
      } catch (e) {
        console.error('[Expander] Error:', e); // Debug
        progress.textContent = `Ошибка: ${e.message}`;
        term.term.writeln(`\r\n[web4] Ошибка: ${e.message}`);
      }
    };
  };

  setupExpanderButton('buttonExpanderFlash0', 'F0', 'D50');
  setupExpanderButton('buttonExpanderFlash1', 'F1', 'D51');
  setupExpanderButton('buttonExpanderFlash2', 'F2', 'D52');
};

// --- Expander tab: trigger the ESP to SWIO-flash its embedded CH32 image ------
// web4 does NOT drive SWIO itself (A11): it sends a 2-byte command over WebSerial
// and the ESP runs the programmer locally against its compiled-in image (A13).
const genOnClickActivateExpander = () => {
  let port = null;

  return async () => {
    const tab = document.getElementById('panel-expander');
    tab.innerHTML = `
      <div class="success">Программатор экспандеров</div>
      <p class="note">Режим программирования (джампер 3.3 В–3.3 В, ESP на USB).
        ESP прошивает встроенный образ по SWIO.</p>
      <p>Статус: <span id="exp-status">не подключено</span></p>
      <div>
        <span class="button flash" data-cmd="F0">Экспандер 0</span>
        <span class="button flash" data-cmd="F1">Экспандер 1</span>
        <span class="button flash" data-cmd="F2">Экспандер 2</span>
        <span class="button flash" data-cmd="FA">Все три</span>
      </div>
      <p>Консоль:</p>
      <pre id="exp-log" style="background:#030;color:#fff;border-radius:10px;padding:10px;height:240px;overflow:auto;white-space:pre-wrap;"></pre>
    `;

    const statusEl = tab.querySelector('#exp-status');
    const logEl = tab.querySelector('#exp-log');
    const log = (s) => { logEl.textContent += s; logEl.scrollTop = logEl.scrollHeight; };
    const flashBtns = [...tab.querySelectorAll('.flash')];
    const setEnabled = (on) => flashBtns.forEach((b) => { b.style.opacity = on ? '1' : '.4'; b.dataset.on = on ? '1' : ''; });
    setEnabled(false);

    try {
      port = await connectEsp({onData: (text) => log(text)});
      statusEl.textContent = 'подключено';
      setEnabled(true);
      log('\n[подключено]\n');
    } catch (e) {
      log('\n[ошибка подключения] ' + e.message + '\n');
      return;
    }

    flashBtns.forEach((btn) => {
      btn.onclick = async () => {
        if (!btn.dataset.on) return;
        const cmd = btn.dataset.cmd;
        setEnabled(false);
        log('\n[отправка ' + cmd + ']\n');
        try {
          await sendCommand(port, cmd);
        } catch (e) {
          log('\n[ошибка отправки] ' + e.message + '\n');
        }
        setEnabled(true);
      };
    });
  };
};

// --- shell -------------------------------------------------------------------
const initHtmlBridge = (hasSerial) => hasSerial ? /*html*/`
  <p>Для начала работы, подключите XAPOH к USB и нажмите кнопку:</p>
  <div class="button" id="buttonBridge">Активировать Мост</div>
` : /*html*/`
  <p>Ваш браузер:</p>
  <p><code>${navigator.userAgent}</code></p>
  <p>не поддерживает WebSerial API</p>
  <p>Попробуйте другой браузер, например Chrome</p>
`;

const initHtml = ($root, genBridge) => {
  $root.innerHTML = /*html*/`
    <div class="header">
      <div class="header-inner">
        <div class="header-title">XAPOH ${manifest.version}</div>
      </div>
    </div>
    <div class="content" id="content">${initHtmlBridge(genBridge)}</div>
  `;
  if (genBridge) {
    document.getElementById('buttonBridge').onclick = genBridge();
  }
};

const onLoad = async () => {
  console.log(manifest.version); // eslint-disable-line no-console
  initCSS();
  const hasSerial = 'serial' in navigator;
  initHtml(document.getElementById('root'), hasSerial && genOnClickActivateBridge
    // hasSerial && genOnClickActivateExpander
  );
};

document.addEventListener('DOMContentLoaded', onLoad);
/* eslint-env browser */
