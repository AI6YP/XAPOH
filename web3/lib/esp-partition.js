// ESP32-C6 partition table + config helpers. Ported from src/components/{t2pt,esp-install}.js.

// ESP-IDF Partition Table for the WiFi bridge (sw-panel-v3).
// partition_table itself lives at 0x8000 (1 flash sector); entries below.
export const partTable = [
  {name: 'nvs',       type: 'data.nvs',       offset: 0x9000,   size: 0x6000},
  {name: 'phy_init',  type: 'data.phy',       offset: 0xf000,   size: 0x1000},
  {name: 'factory',   type: 'app.factory',    offset: 0x10000,  size: 0x100000},
  {name: 'config',    type: 'data.undefined', offset: 0x110000, size: 0x2000}
];

// Serialize the user wifi config into the `config` partition image.
// Layout: ssid @ offset 0, password @ offset 32. Editor emits {wifi:{ssid,password}}.
export const cfg2ui8 = (cfg) => {
  const totalLength = partTable.find((e) => e.name === 'config').size;
  const resU8 = new Uint8Array(totalLength); // all zeros

  const ssid = cfg?.wifi?.ssid || '';
  for (let i = 0; i < ssid.length; i++) {
    resU8[i] = ssid.charCodeAt(i);
  }
  const password = cfg?.wifi?.password || '';
  for (let i = 0; i < password.length; i++) {
    resU8[32 + i] = password.charCodeAt(i);
  }
  return resU8;
};

// Build the ESP-IDF partition-table.bin from a table description.
// `md5` is an async fn: (Uint8Array) -> Uint8Array (16 bytes).
export const t2pt = async (table, md5) => {

  const typo = {
    'data':             0x01,

    'data.ota':       0x0101,
    'data.phy':       0x0101,
    'data.nvs':       0x0201,
    'data.coredump':  0x0301,
    'data.nvs_keys':  0x0401,
    'data.efuse':     0x0501,
    'data.undefined': 0x0601,
    'data.esphttpd':  0x8001,
    'data.fat':       0x8101,
    'data.spiffs':    0x8201,
    'data.littlefs':  0x8301,

    'app':              0x00,
    'app.factory':    0x0000,
    'app.ota_0':      0x1000,
    'app.ota_1':      0x1100,
    'app.ota_2':      0x1200,
    'app.ota_3':      0x1300,
    'app.ota_4':      0x1400,
    'app.ota_5':      0x1500,
    'app.ota_6':      0x1600,
    'app.ota_7':      0x1700,
    'app.ota_8':      0x1800,
    'app.ota_9':      0x1900,
    'app.ota_10':     0x1A00,
    'app.ota_11':     0x1B00,
    'app.ota_12':     0x1C00,
    'app.ota_13':     0x1D00,
    'app.ota_14':     0x1E00,
    'app.ota_15':     0x1F00,
    'app.test':       0x2000,

    'bootloader':             0x02,
    'bootloader.primary':   0x0002,
    'bootloader.ota':       0x0102,
    'bootloader.recovery':  0x0202,

    'partition_table':            0x03,
    'partition_table.primary':  0x0003,
    'partition_table.ota':      0x0103
  };

  const bufSize = 0xC00;
  const buff = new ArrayBuffer(bufSize);
  const a8  = new Uint8Array(buff);
  const a16 = new Uint16Array(buff);
  const a32 = new Uint32Array(buff);

  a8.fill(0xff);

  table.map((e, i) => {
    a16[i * 16] = 0x50AA;
    a16[i * 16 + 1] = typo[e.type];
    a32[i * 8 + 1] = e.offset;
    a32[i * 8 + 2] = e.size;
    for (let j = 0; j < 20; j++) {
      a8[i * 32 + 12 + j] = e.name.charCodeAt(j);
    }
  });

  a16[table.length * 16] = 0xEBEB;
  const fullData = a8.slice(0, table.length * 32);

  const hash = await md5(fullData);

  a8.set(hash, table.length * 32 + 16);

  return a8;
};
