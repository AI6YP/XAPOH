'use strict';

const stringify = require('onml/stringify.js');

// eslint-disable-next-line no-undef
const manifest = MANIFEST;

// v4 WS protocol (operation mode, over WiFi):
//   [target(0..2), b0, b1, b2]  -> expander full-state push
//   [255, ...leds]              -> RGB strip
// Each panel config carries 9 output bytes = 3 expanders x 3 bytes (g0..g8).
const sendGpio = (socket, configs) => {
  let [g0, g1, g2, g3, g4, g5, g6, g7, g8] = [0, 0, 0, 0, 0, 0, 0, 0, 0];
  let leds = Array.from({length: 10 * 3}, () => 0);
  let bandIdx;
  configs.map((cfg, idx) => {
    if (cfg.val && cfg.gpios) {
      [g0, g1, g2, g3, g4, g5, g6, g7, g8] = cfg.gpios;
      bandIdx = idx - 1;
      leds[1] = 100; // green 0
    }
  });
  configs.map((cfg) => {
    if (cfg.val) {
      if (cfg.name === 'ON') {
        g7 = g7 | 8; // D52 "ON" signal
        leds[0] = 100; // red 0
        if (bandIdx) {
          leds[bandIdx * 3 + 2] = 100; // blue
        }
      } else if (cfg.name === 'PreAmp') {
        g7 = g7 | 4; // D52 "PREAMP" signal
        leds[2] = 100; // blue 0
        if (bandIdx) {
          leds[bandIdx * 3 + 1] = 100; // red (green)
        }
      }
    }
  });
  console.log(g0, g1, g2, g3, g4, g5, g6, g7, g8);
  if (socket) {
    socket.send(Uint8Array.from([0, g0, g1, g2])); // expander 0
    socket.send(Uint8Array.from([1, g3, g4, g5])); // expander 1
    socket.send(Uint8Array.from([2, g6, g7, g8])); // expander 2
    socket.send(Uint8Array.from([255, ...leds]));
  }
};

function bf (arr) {
  let res = 0;
  for (const [idx, e] of arr.entries()) {
    if (e) {
      res += 2 ** idx;
    }
  }
  return res;
}

const onLoad = async () => {
  let t0;
  console.log(manifest.version); // eslint-disable-line no-console
  const content = document.getElementById('root');
  const font = new FontFace('Iosevka Drom', 'url(https://vc.drom.io/IosevkaDrom-Regular.woff2)');
  document.fonts.add(font);
  await font.load();

  const ml = (state) => ['div', {class: 'xapohapp'},
    ['h1', 'XAPOH'],
    ['div', {class: 'configs'},
      ...state.configs.flatMap((config, i) => [
        ['input', {id: `send_cfg${i}`, class: 'btn_item', type: 'button', value: config.name}]
      ])
    ],
    ['div', {class: 'tiny'}, manifest.version]
  ];

  const state = {
    configs: [ /* eslint no-sparse-arrays :0, array-bracket-spacing: 0, comma-spacing: 0, comma-dangle: 0 */
      //                              //                     (D50)                                                  (D51)                                                  (D52)                              .
      //                              //                                                                                                                                                                      .
      //                              //            Y   Y                H   Y                         T     T                    T     T                         T       T            P     T   T            .
      {name: 'ON',     g: 'on'     }, //        J Y 3 Q 1                1 B 6 Y Y Y        H        Y 3 T T 2 Y   Z          T   7 T T 6 Y        N        N N T 4 N Q   8            R O T 1 T 5        N   .
      {name: 'PreAmp', g: 'preamp' }, //      B N 1 N N N   Q            N N N 5 3 6        1        9 N 3 2 N 2   2          5 J N 7 6 N 4        5        4 3 8 N 1 +   N            E N 1 N 4 N        2   .
      {name: 'HF',     g: 'band', gpios: [bf([ , , , , , , , ]), bf([ , , , , , , , ]), bf([1]), bf([ ,1, , ,1, , , ]), bf([ ,1,1,1, , ,1, ]), bf([1]), bf([ , , , , , , ,1]), bf([ , , , , ,1,1, ]), bf([ ])]},
      {name: '144',    g: 'band', gpios: [bf([ , , , ,1,1, , ]), bf([ , ,1,1,1, ,1, ]), bf([ ]), bf([ ,1, , ,1, , , ]), bf([ ,1,1,1, , ,1, ]), bf([ ]), bf([1,1, , , , , ,1]), bf([ , , , , ,1,1, ]), bf([1])]},
      {name: '432',    g: 'band', gpios: [bf([ , ,1,1,1, , , ]), bf([ , ,1,1,1, , , ]), bf([ ]), bf([ ,1, , ,1, , , ]), bf([ ,1,1,1, , ,1, ]), bf([ ]), bf([1,1, ,1,1, , ,1]), bf([ , , , ,1, , , ]), bf([ ])]},
      {name: '1296',   g: 'band', gpios: [bf([ , , ,1,1,1, , ]), bf([ , ,1,1,1, , , ]), bf([ ]), bf([ ,1, ,1, ,1, , ]), bf([ , ,1,1, , ,1, ]), bf([ ]), bf([1, ,1,1,1, , , ]), bf([ , , , , ,1, ,1]), bf([1])]},
      {name: '2400',   g: 'band', gpios: [bf([ , , ,1,1,1, , ]), bf([ , ,1,1,1, , , ]), bf([ ]), bf([1, ,1, ,1, , , ]), bf([ , ,1, ,1,1, ,1]), bf([ ]), bf([ ,1,1,1,1, , , ]), bf([ , , , , ,1, ,1]), bf([1])]},
      {name: 'SAT A',  g: 'band', gpios: [bf([ , , , ,1,1, , ]), bf([ , ,1,1, ,1,1,1]), bf([ ]), bf([ ,1, , ,1, , , ]), bf([ ,1,1,1, , ,1, ]), bf([ ]), bf([1,1,1, , , , , ]), bf([ , , , , ,1,1, ]), bf([1])]},
      {name: 'SAT B',  g: 'band', gpios: [bf([1,1,1,1,1, , , ]), bf([ , ,1, , ,1, ,1]), bf([ ]), bf([ ,1, , ,1, , , ]), bf([ ,1, ,1, , ,1, ]), bf([ ]), bf([1,1,1,1,1, , , ]), bf([ , , , ,1, , , ]), bf([ ])]},
      {name: 'SAT J',  g: 'band', gpios: [bf([ ,1, , ,1,1, , ]), bf([ , ,1,1, ,1,1,1]), bf([ ]), bf([ ,1, , ,1, , , ]), bf([ ,1, ,1, , ,1, ]), bf([ ]), bf([1,1,1, , , , , ]), bf([ , , , , ,1,1, ]), bf([1])]},
      {name: 'SAT Q',  g: 'band', gpios: [bf([ , , ,1, ,1, ,1]), bf([ , ,1,1, , , ,1]), bf([ ]), bf([ , ,1, ,1, , , ]), bf([ , ,1,1, ,1, ,1]), bf([ ]), bf([ ,1,1,1,1,1, , ]), bf([ , , , , ,1, ,1]), bf([1])]},
    ]
  };

  content.innerHTML = stringify(ml(state));

  let socket;
  socket = new WebSocket('ws://' + location.host + '/dev1');
  socket.binaryType = 'arraybuffer';
  await new Promise((resolve) => {
    socket.addEventListener('open', () => resolve());
  });
  socket.addEventListener('message', (event) => {
    const u8 = new Uint8Array(event.data);
    let str = '< ';
    for (const c of u8) {
      str += c.toString(16).padStart(2, 0) + ' ';
    }
    str += '>';
    console.log('Message from server: ' + str + ' (' + (performance.now() - t0) + ')');
  });

  state.configs.map((config, i) => {
    const id = `send_cfg${i}`;
    const el = document.getElementById(id);
    config.target = el;
    el.addEventListener('click', () => {
      state.configs.map((cfg) => {
        if (cfg.target === config.target) {
          config.val = !config.val;
          if (config.val) {
            config.target.classList.add('active');
          } else {
            config.target.classList.remove('active');
          }
        } else if (cfg.g === config.g) {
          cfg.target.classList.remove('active');
          cfg.val = false;
        }
      });
      console.log(config);
      sendGpio(socket, state.configs);
    });
  });

  window.SOCKET = {
    send: (msg) => {
      t0 = performance.now();
      socket.send(msg);
    }
  };
};

document.addEventListener('DOMContentLoaded', onLoad);
/* eslint-env browser */
