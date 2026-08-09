import { ESPLoader, Transport, CustomReset } from 'esptool-js';
import { md5 } from 'js-md5';
import { xterm } from './xterm.js';
import { partTable, t2pt, cfg2ui8 } from './esp-partition.js';
import { mountBridgeConfigEditor } from './mount-bridge-config-editor.js';
import { initCSS } from './init-css.js';

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
  const activateBtn = document.getElementById('buttonBridge');
  if (activateBtn) {
    activateBtn.setAttribute('aria-disabled', 'true');
    activateBtn.textContent = 'Подключение…';
  }
  let port, transport, esploader, chip, term;
  try {
    const filters = [{usbVendorId: 0x303a, usbProductId: 0x1001}]; // ESP32-C6
    port = await navigator.serial.requestPort({filters});
    transport = new Transport(port, true);
    term = xterm();
    const baudrate = [115200, 460800, 921600][1];
    const flashOptions = {transport, baudrate, terminal: term.callbacks};
    esploader = new ESPLoader(flashOptions);
    chip = await esploader.main();
  } catch (e) {
    if (activateBtn) {
      activateBtn.removeAttribute('aria-disabled');
      activateBtn.textContent = 'Активировать XAPOH';
    }
    return;
  }
  console.log(chip); // eslint-disable-line no-console

  const tab = document.getElementById('content');
  tab.innerHTML = /*html*/`
    <div class="success">XAPOH подключен: ${chip}</div>
    <h3>Конфигурация</h3>
    <div class="bridge-config" id="bridgeConfig"></div>
    <div class="flash-row">
      <span class="button flush" id="buttonBridgeFlash" role="button" tabindex="0">Прошить ESP32C6</span>
      <span class="button flash" id="buttonExpanderFlash0" role="button" tabindex="0">Прошить D50</span>
      <span class="button flash" id="buttonExpanderFlash1" role="button" tabindex="0">Прошить D51</span>
      <span class="button flash" id="buttonExpanderFlash2" role="button" tabindex="0">Прошить D52</span>
    </div>
    <div id="bridge-progress">
      <div class="progress-label"></div>
      <progress value="0" max="0"></progress>
    </div>
    <p>Консоль: <span class="button clear-console" id="buttonClearConsole" role="button" tabindex="0">Очистить</span></p>
    <div id="console"></div>
  `;
  mountBridgeConfigEditor(tab.querySelector('#bridgeConfig'));

  const progress = tab.querySelector('#bridge-progress');
  const progressLabel = progress.querySelector('.progress-label');
  const progressEl = progress.querySelector('progress');
  let consoleWriter = null; // Store the writer for reuse by expander buttons
  let consoleInitialized = false; // Track if console has been initialized
  let onDataDisposer = null; // Dispose previous term.onData listener on re-init

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
    if (on) {
      b.removeAttribute('aria-disabled');
    } else {
      b.setAttribute('aria-disabled', 'true');
    }
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

  // Clear-console button (fix 7): clears the xterm buffer without dropping
  // the serial connection.
  const clearBtn = tab.querySelector('#buttonClearConsole');
  const clearConsole = () => term.term.clear();
  clearBtn.addEventListener('click', clearConsole);
  clearBtn.addEventListener('keydown', (e) => {
    if (e.key === 'Enter' || e.key === ' ') {
      e.preventDefault();
      clearConsole();
    }
  });

  // Handle USB unplug (fix 6): WebSerial fires 'disconnect' on the port (it
  // bubbles to navigator.serial). Surface it to the user and disable the D5x
  // buttons so no further sends go to a dead port.
  const onDisconnect = () => {
    term.term.writeln('\r\n[web4] соединение потеряно');
    progressLabel.textContent = 'Соединение потеряно (USB отключён)';
    setExpanderEnabled(false);
  };
  navigator.serial.addEventListener('disconnect', (e) => {
    if (e.target === port) onDisconnect();
  });

  // Switch to serial mode and start reading (called when expander button is pressed or after ESP32C6 flash)
  const initConsole = async () => {
    if (consoleInitialized) return; // Already initialized
    try {
      await transport.disconnect();
      await transport.connect(115200); // CONFIG_ESP_CONSOLE_UART_BAUDRATE

      consoleWriter = transport.device.writable.getWriter();
      // Dispose any prior onData listener before registering a new one (fix 3):
      // each flash + re-init would otherwise stack another listener and echo
      // every keystroke N times.
      if (onDataDisposer) onDataDisposer.dispose();
      onDataDisposer = term.term.onData((data) => consoleWriter.write(new TextEncoder().encode(data)));
      consoleInitialized = true;
      await transport.rawRead(
        (value) => term.term.write(value),
        () => false
      );
    } catch (e) {
      progressLabel.textContent = 'Ошибка инициализации консоли: ' + e.message;
    }
  };

  const flashBtn = tab.querySelector('#buttonBridgeFlash');
  const flashBridge = async () => {
    if (flashBtn.getAttribute('aria-disabled') === 'true') return; // one-shot
    flashBtn.setAttribute('aria-disabled', 'true');
    let cfg;
    try {
      cfg = JSON.parse(localStorage.getItem('bridgeConfig') || '{}');
    } catch (e) {
      progressLabel.textContent = 'ОШИБКА: неверный JSON конфигурации';
      flashBtn.removeAttribute('aria-disabled');
      return;
    }

    const fileArray = await buildFileArray(cfg);
    await esploader.writeFlash({
      fileArray,
      flashSize: 'keep',
      eraseAll: false,
      compress: true,
      reportProgress: (fileIndex, written, total) => {
        progressLabel.textContent = `Прошивка ${fileIndex + 1}/${fileArray.length}: ${written}/${total}`;
        progressEl.max = total;
        progressEl.value = written;
      }
    });
    progressLabel.textContent = 'Прошивка завершена. Перезагрузка…';
    progressEl.value = progressEl.max;
    setExpanderEnabled(true);
    // flashBtn stays disabled — the ESP32C6 is flashed and rebooting; the D5x
    // buttons are now the active controls. Re-flashing needs a fresh connect.

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
      progressLabel.textContent = 'Готово. Переподключите USB для консоли.';
    }
  };
  flashBtn.onclick = flashBridge;
  flashBtn.addEventListener('keydown', (e) => {
    if (e.key === 'Enter' || e.key === ' ') {
      e.preventDefault();
      flashBridge();
    }
  });

  // --- Expander flash buttons: send F0/F1/F2 commands to ESP via serial --------
  // Each button disables itself (and its siblings) during the send (fix 2) —
  // matches the expander tab's gating, so a second click can't re-trigger an
  // in-progress SWIO flash.
  const setupExpanderButton = (buttonId, cmd, name) => {
    const btn = tab.querySelector(`#${buttonId}`);
    if (!btn) return;

    const trigger = async () => {
      if (btn.getAttribute('aria-disabled') === 'true') {
        return; // disabled until ESP32C6 flash completes / during a send
      }
      setExpanderEnabled(false);
      term.term.writeln(`\r\n[web4] Прошивка ${name}, команда: ${cmd}`);
      progressLabel.textContent = `Прошивка ${name}...`;

      try {
        // Initialize console if not already done
        if (!consoleWriter) {
          term.term.writeln('[web4] Инициализация консоли...');
          progressLabel.textContent = 'Инициализация консоли...';
          await initConsole();
          // Wait a bit for console to be ready
          await new Promise(resolve => setTimeout(resolve, 500));
        }

        if (!consoleWriter) {
          throw new Error('Console writer not initialized');
        }

        term.term.writeln(`[web4] Отправка команды: ${cmd}`);
        await consoleWriter.write(new TextEncoder().encode(cmd));
        progressLabel.textContent = `Команда ${cmd} отправлена для ${name}.`;
        term.term.writeln(`[web4] Команда отправлена`);
      } catch (e) {
        console.error('[Expander] Error:', e); // eslint-disable-line no-console
        progressLabel.textContent = `Ошибка: ${e.message}`;
        term.term.writeln(`\r\n[web4] Ошибка: ${e.message}`);
      } finally {
        // Re-enable only if the ESP32C6 flash has already completed (the D5x
        // buttons are otherwise gated on flash completion). If a disconnect
        // happened mid-send, keep them disabled.
        if (expanderBtns.every((b) => b && b.getAttribute('aria-disabled') === 'true')
            && progressLabel.textContent.startsWith('Команда')) {
          setExpanderEnabled(true);
        }
      }
    };

    btn.onclick = trigger;
    btn.addEventListener('keydown', (e) => {
      if (e.key === 'Enter' || e.key === ' ') {
        e.preventDefault();
        trigger();
      }
    });
  };

  setupExpanderButton('buttonExpanderFlash0', 'F0', 'D50');
  setupExpanderButton('buttonExpanderFlash1', 'F1', 'D51');
  setupExpanderButton('buttonExpanderFlash2', 'F2', 'D52');
};

// --- shell -------------------------------------------------------------------
const initHtmlBridge = (hasSerial) => hasSerial ? /*html*/`
  <p>Для начала работы, подключите XAPOH к USB и нажмите кнопку:</p>
  <div class="button" id="buttonBridge" role="button" tabindex="0">Активировать XAPOH</div>
` : /*html*/`
  <p>Ваш браузер:</p>
  <p><code>${navigator.userAgent}</code></p>
  <p>не поддерживает WebSerial API</p>
  <p>Попробуйте другой браузер, например Chrome</p>
`;

const initHtml = ($root, hasSerial) => {
  $root.innerHTML = /*html*/`
    <div class="header">
      <div class="header-inner">
        <div class="header-title">XAPOH ${manifest.version}</div>
      </div>
    </div>
    <div class="content" id="content">${initHtmlBridge(hasSerial)}</div>
  `;

  if (hasSerial) {
    // Wire up the Activate button. genOnClickActivateBridge returns the async
    // handler; the button disables itself (aria-disabled + "Подключение…") on
    // click before the async requestPort/esptool.main() (fix 1).
    const bridgeBtn = document.getElementById('buttonBridge');
    const bridgeHandler = genOnClickActivateBridge();
    bridgeBtn.addEventListener('click', bridgeHandler);
    bridgeBtn.addEventListener('keydown', (e) => {
      if (e.key === 'Enter' || e.key === ' ') {
        e.preventDefault();
        bridgeHandler();
      }
    });
  }
};

const onLoad = async () => {
  console.log(manifest.version); // eslint-disable-line no-console
  initCSS();
  const hasSerial = 'serial' in navigator;
  initHtml(document.getElementById('root'), hasSerial);
};

document.addEventListener('DOMContentLoaded', onLoad);
/* eslint-env browser */
