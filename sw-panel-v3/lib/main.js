'use strict';

const pkg = require('../package.json');
const stringify = require('onml/stringify.js');

const sendGpio = (socket, config) => {
  console.log(config);
  socket.send(Uint8Array.from([0, 0, config.gpios[0], config.gpios[1]]));
  socket.send(Uint8Array.from([1, 0, config.gpios[2], config.gpios[3]]));
};

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
        ['div', {class: 'item'}, config.gpios.join('.')],
        // ['input', {class: 'btn_item del', type: 'button', value: '\u2715'}]
      ]),
      // ['input', {class: 'num_item', type: 'text'}],
      // ['input', {class: 'num_item', type: 'text'}],
      // ['input', {class: 'btn_item add', type: 'button', value: '+'}]
    ],
    ['div', {class: 'tiny'}, pkg.version]
  ];
  // btns.map(btno => {
  //   ml.push(['button', {id: btno.id}, btno.label]);
  // });

  const state = {
    configs: [
      {name: '144',   gpios: [6,    125,  8,    4]},
      {name: '432',   gpios: [66,   93,   16,   36]},
      {name: '1296',  gpios: [38,   125,  16,   64]},
      {name: '2400',  gpios: [102,  61,   20,   4]},
      {name: 'SAT A', gpios: [5,    109,  10,   5]},
      {name: 'SAT B', gpios: [10,   69,   18,   37]},
      {name: 'SAT J', gpios: [6,    107,  10,   5]},
      {name: 'SAT Q', gpios: [6,    60,   84,   4]},
    ]
  };

  content.innerHTML = stringify(ml(state));

  const socket = new WebSocket('ws://' + location.host + '/dev1');
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

  state.configs.map((config, i) => {
    const id = `send_cfg${i}`;
    const el = document.getElementById(id);
    el.addEventListener('click', () => {
      sendGpio(socket, config);
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
