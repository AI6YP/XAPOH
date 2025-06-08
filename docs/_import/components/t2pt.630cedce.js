export const t2pt = async (table, md5) => {

  const typo = {
    'data':             0x01, // bootloaded

    'data.ota':       0x0101, // size: 0x2000
    'data.phy':       0x0101, // default
    'data.nvs':       0x0201, // default
    'data.coredump':  0x0301, // for storing core dumps while using a custom partition table CSV file. See Core Dump for more details.
    'data.nvs_keys':  0x0401,
    'data.efuse':     0x0501, // for emulating eFuse bits using Virtual eFuses.
    'data.undefined': 0x0601, // implicitly used for data partitions with unspecified (empty) subtype, but it is possible to explicitly mark them as undefined as well.
    'data.esphttpd':  0x8001,
    'data.fat':       0x8101, // for FAT Filesystem Support.
    'data.spiffs':    0x8201, // for SPIFFS Filesystem.
    'data.littlefs':  0x8301, // for LittleFS filesystem. See storage/littlefs example for more details.

    'app':              0x00, // bootloaded
    'app.factory':    0x0000, // default
    // OTA data partition configures which app slot the bootloader should boot.
    // When using OTA, an application should have at least two OTA application
    // slots (ota_0 & ota_1).
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
    'app.test':       0x2000, // factory test

    // not included
    'bootloader':             0x02,
    'bootloader.primary':   0x0002,
    // temporary bootloader partition used by the bootloader OTA update
    // functionality for downloading a new image.
    'bootloader.ota':       0x0102,
    'bootloader.recovery':  0x0202,

    'partition_table':            0x03,
    'partition_table.primary':  0x0003,
    // It is a temporary partition table partition used by the partition table
    // OTA update functionality for downloading a new image.
    'partition_table.ota':      0x0103,

    //  0x40...0xfe   reserved
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
