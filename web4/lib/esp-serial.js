// Minimal WebSerial link to the ESP32-C6 (USB-Serial-JTAG CDC).
// web4 only triggers flashing — the ESP runs the SWIO programmer locally and
// flashes its embedded CH32V003 image (see sw-panel-v4 flash_cmd_task: 'F' + 0/1/2/A).

const enc = new TextEncoder();

// Open a serial port and stream incoming text to onData. Returns the port.
export const connectEsp = async ({onData = () => {}, baudRate = 115200} = {}) => {
  if (!('serial' in navigator)) {
    throw new Error('WebSerial unavailable (use Chrome/Edge over https or localhost)');
  }
  const port = await navigator.serial.requestPort();
  await port.open({baudRate});

  // Background read loop: decode ESP_LOG output and forward to the UI.
  (async () => {
    const dec = new TextDecoder();
    try {
      while (port.readable) {
        const reader = port.readable.getReader();
        try {
          for (;;) {
            const {value, done} = await reader.read();
            if (done) break;
            if (value) onData(dec.decode(value, {stream: true}));
          }
        } finally {
          reader.releaseLock();
        }
      }
    } catch (e) {
      onData('\n[read stopped] ' + e.message + '\n');
    }
  })();

  return port;
};

// Send a 2-char command ('F0'|'F1'|'F2'|'FA') to the ESP.
export const sendCommand = async (port, cmd) => {
  if (!port || !port.writable) throw new Error('not connected');
  const writer = port.writable.getWriter();
  try {
    await writer.write(enc.encode(cmd));
  } finally {
    writer.releaseLock();
  }
};
/* eslint-env browser */
