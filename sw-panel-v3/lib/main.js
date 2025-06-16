'use strict';

const stringify = require('onml/stringify.js');

const sendGpio = (socket, configs) => {
  let [g0, g1, g2, g3] = [0, 0, 0, 0]; // config.gpios;
  let leds = Array.from({length: 9 * 3}, () => 0);
  let bandIdx;
  configs.map((cfg, idx) => {
    if (cfg.val && cfg.gpios) {
      [g0, g1, g2, g3] = cfg.gpios;
      bandIdx = idx - 1; // 1..8
      // leds[bandIdx * 3 + 2] = 100; // blue
      leds[1] = 100; // green 0
    }
  });
  configs.map((cfg) => {
    if (cfg.val) {
      if (cfg.name === 'ON') {
        g2 += 16;
        leds[0] = 100; // red 0
        if (bandIdx) {
          leds[bandIdx * 3 + 2] = 100; // blue 
        }
      } else if (cfg.name === 'PreAmp') {
        g3 += 8;
        leds[2] = 100; // blue 0
        if (bandIdx) {
          leds[bandIdx * 3 + 1] = 100; // red (green)
        }
      }
    }
  });
  console.log(g0, g1, g2, g3);
  if (socket) {
    socket.send(Uint8Array.from([0, 0, g0, g1]));
    socket.send(Uint8Array.from([1, 0, g2, g3]));
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

global.XAPOH = async (divName) => {
  let t0;
  const content = document.getElementById(divName);

  const font = new FontFace('Iosevka Drom', 'url(https://vc.drom.io/IosevkaDrom-Regular.woff2)');
  document.fonts.add(font);
  await font.load();

  const ml = (state) => ['div', {class: 'xapohapp'},
    ['h1', 'XAPOH'],
    ['div', {class: 'configs'},
      ...state.configs.flatMap((config, i) => [
        ['input', {id: `send_cfg${i}`, class: 'btn_item', type: 'button', value: config.name}],
        // ['input', {class: 'item', type: 'text', value: config.gpios.join('.')}],
        // ['div', {class: 'item'}, config.gpios.join('.')],
        // ['input', {class: 'btn_item del', type: 'button', value: '\u2715'}]
      ]),
      // ['input', {class: 'num_item', type: 'text'}],
      // ['input', {class: 'num_item', type: 'text'}],
      // ['input', {class: 'btn_item add', type: 'button', value: '+'}]
      // ['input', {id: 'send_lna', class: 'btn_item', type: 'button'}],
    ],
    ['div', {class: 'tiny'}, document.XAPOH_VERSION]
  ];
  // btns.map(btno => {
  //   ml.push(['button', {id: btno.id}, btno.label]);
  // });

  const state = {
    configs: [
      //                                   //              (D17)             Y                       (D23)
      //                                   // Y   Y                        Y 1 Y                  Y          Y Y Y A       
      {name: 'ON',     g: 'on'     },      // 3   1     M M            J B 5 N 4        Y Y Y Y O 6 Q        5 7 2 M Y Y Y
      {name: 'PreAmp', g: 'preamp' },      // N . N B . A B        Q J N N N N N        5 6 4 3 N N N        + N N P 7 1 2 
      {name: '144',    g: 'band', gpios: [bf([ , ,1, , , , ]), bf([ ,1, ,1,1,1,1]), bf([ , , ,1, ,1,1]), bf([ ,1,1, , , , ])]},
      {name: '432',    g: 'band', gpios: [bf([1, , , , , ,1]), bf([ ,1, ,1,1, ,1]), bf([ , , , , ,1,1]), bf([ ,1,1, , ,1, ])]},
      {name: '1296',   g: 'band', gpios: [bf([1, ,1, , ,1, ]), bf([ ,1, ,1,1,1,1]), bf([ , ,1, , ,1,1]), bf([ , , , ,1, ,1])]},
      {name: '2400',   g: 'band', gpios: [bf([1, ,1, , ,1,1]), bf([ ,1, ,1,1,1, ]), bf([ , ,1, , ,1,1]), bf([ , ,1, ,1, , ])]},
      {name: 'SAT A',  g: 'band', gpios: [bf([ , ,1, , , , ]), bf([ ,1, ,1, ,1,1]), bf([1,1, ,1, , ,1]), bf([1,1,1, , , , ])]},
      {name: 'SAT B',  g: 'band', gpios: [bf([1, , ,1, , , ]), bf([ ,1, , , , ,1]), bf([1,1, , , , ,1]), bf([1,1,1, , ,1, ])]},
      {name: 'SAT J',  g: 'band', gpios: [bf([ , ,1, , , , ]), bf([ , ,1,1, ,1,1]), bf([1,1, ,1, , ,1]), bf([1,1,1, , , , ])]},
      {name: 'SAT Q',  g: 'band', gpios: [bf([1, ,1, , , , ]), bf([1,1, ,1,1,1, ]), bf([ ,1,1, , , , ]), bf([ , ,1, ,1, , ])]},
    ]
  };

  content.innerHTML = stringify(ml(state));

  let socket;

  if (true) {
    socket = new WebSocket('ws://' + location.host + '/dev1');
    socket.binaryType = 'arraybuffer';

    // Connection opened
    await new Promise((resolve, reject) => {
      socket.addEventListener('open', (event) => {
        resolve();
      });
    });

    // Listen for messages
    socket.addEventListener('message', (event) => {
      const u8 = new Uint8Array(event.data);
      let str = '< ';
      for (const c of u8) {
        str += c.toString(16).padStart(2, 0) + ' ';
      }
      str += '>';
      console.log('Message from server: ' + str + ' (' + (performance.now() - t0) + ')');
    });
  }

  state.configs.map((config, i) => {
    const id = `send_cfg${i}`;
    const el = document.getElementById(id);
    config.target = el;
    el.addEventListener('click', () => {
      state.configs.map((cfg, i) => {
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

  global.SOCKET = {
    send: (msg) => {
      t0 = performance.now();
      socket.send(msg);
    }
  };
};

/* eslint-env browser */
