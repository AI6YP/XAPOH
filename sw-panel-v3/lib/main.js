'use strict';

const stringify = require('onml/stringify.js');

global.XAPOH = async (divName) => {
  let t0;
  const content = document.getElementById(divName);

  // p2k groups
  // const btns = [
  //   {id: 'btn_start', label: 'Start', cmd: 0},
  //   {id: 'btn_stop',  label: 'Stop',  cmd: 1},
  //   {id: 'btn_pause', label: 'Pause', cmd: 2}
  // ];

  const ml = (state) => ['div', {class: 'xapohapp'},
    ['h1', 'XAPOH'],
    ['div', {class: 'configs'},
      ...state.configs.flatMap((config, i) => [
        ['input', {id: `send_cfg${i}`, class: 'btn_item', type: 'button', value: config.name}],
        ['span',  {class: 'item'}, config.gpios.join('.')],
        ['input', {class: 'btn_item del', type: 'button', value: '\u2715'}]
      ]),
      ['input', {class: 'num_item', type: 'text'}],
      ['input', {class: 'num_item', type: 'text'}],
      ['input', {class: 'btn_item add', type: 'button', value: '+'}]
    ]
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

  state.configs.map((config, i) => {
    const id = `send_cfg${i}`;
    const el = document.getElementById(id);
    el.addEventListener('click', (event) => {
      console.log(config);
      socket.send(Uint8Array.from([0, 0, config.gpios[0], config.gpios[1]]));
      socket.send(Uint8Array.from([1, 0, config.gpios[2], config.gpios[3]]));
    });
  });

  // const elo = ['i2c_address', 'i2c_data', 'i2c_send'].reduce((res, id) => {
  //   res[id] = document.getElementById(id);
  //   return res;
  // }, {});

  // elo.i2c_send.addEventListener('click', (event) => {
  //   const arr = elo.i2c_data.value.split(',').map(e => Number(e));
  //   const txMessage = Uint8Array.from([
  //     Number(elo.i2c_address.value),
  //     ...arr
  //   ]);
  //   console.log(txMessage);
  //   socket.send(txMessage);
  // });

  // btns.map(btno => {
  //   document.getElementById(btno.id).addEventListener('click', (event) => {
  //     console.log(btno.label, event);
  //     socket.send(btno.cmd);
  //   });
  // });

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

  global.SOCKET = {
    send: (msg) => {
      t0 = performance.now();
      socket.send(msg);
    }
  };
};

/* eslint-env browser */
