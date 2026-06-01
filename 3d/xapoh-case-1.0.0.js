'use strict';

const { makePolygon, makeSolid, makeBaseBox, makeCylinder, makeBox, draw } = replicad;

const defaultParams = {};

const main = (r, $) => {
  // XAPOH PCB
  const xapoh0 = [51.5, -25, 0];
  // const xapoh1 = [150, -110];
  const xapohWH = [98.5, 85];
  const H = 48; // 51-3
  const D = 145;
  const WD = 125;
  const DE = 15;
  const WE = 19;
  const P16 = 1.6;

  let xapoh = makeBox([0, 0, 0], [xapohWH[0], -xapohWH[1], 0.6]);

  const bolts = [
    [ 55.00,  -28.50], [ 96.00,  -29.00], [146.90,  -28.10],
    [ 56.00, -105.50], [ 99.05, -104.55], [145.55, -105.55],
  ].map((pos) => [pos[0] - xapoh0[0], pos[1] - xapoh0[1]]);

  const radiatorBolts = [
    [112.37,  -48.09], [112.62,  -66.59], [112.62,  -57.09]
  ].map((pos) => [pos[0] - xapoh0[0], pos[1] - xapoh0[1]]);

  xapoh = [...bolts, ...radiatorBolts]
    .reduce((res, point) => res.cut(makeCylinder(1.6, 2, point)), xapoh);

  xapoh = xapoh.fuse(makeCylinder((130.62 - 94.62) / 2, 12, [(130.62 + 94.62) / 2 - xapoh0[0], -(75.08 + 39.09) / 2 - xapoh0[1], -12]))

  xapoh = xapoh.translate(WE, -DE, H - 10);

  let ekran = makeBox([0, 0, 0], [xapohWH[0], -xapohWH[1], 1]);
  ekran = bolts.reduce((res, pos) => res.fuse(makeCylinder(2, 12-0.6, pos).translateZ(0.6)), ekran);
  ekran = ekran.translate(WE, -DE, H-10-12);

  // North Panel
  let NP = makeBox([0, 0, 0], [WD, 1.6, H+2*P16]);
  NP = NP.translate(0, 0, -P16);

  // South Panel
  let SP = makeBox([0, 0, 0], [WD, 1.6, H+2*P16]);
  SP = SP.translate(0, -D-P16, -P16);

  // West side
  let W = makeBox([0, 0, 0], [6, -D, H]);

  // East side
  let E = makeBox([0, 0, 0], [6, -D, H]);
  E = E.translate(WD-6);

  // North side
  let N = makeBox([0, 0, 0], [WD, 18, 4]).translate(0, -18, H-4);
  N = N.fuse(makeBox([0, 0, 0], [WD, 2, 12]).translate(0, -12, H-12));
  N = N.fuse(makeBox([0, 0, 0], [WD, 2, 26]).translate(0, -12));
  N = N.fuse(makeBox([0, 0, 0], [WD, 12, 4]).translate(0, -12));

  // South side
  let S = makeBox([0, 0, 0], [WD, 6, 6]);
  S = S.fuse(makeBox([0, 0, 0], [18, 6, -30]));
  S = S.translate(0, -DE-87, H-6);

  let frame = W
    .fuse(E)
    .fuse(N)
    .fuse(S);

  frame = frame.chamfer(0.3);

  frame = Array.from({length: 17}, (_, i) => i).reduce((res, idx) =>
    res.fuse(makeBaseBox(1.5, 12, H).translate(idx * 6.944 + 5.611, -6)), frame);


  // xapoh mount columns
  frame = bolts
    .map((pos) => [pos[0] + WE, pos[1] - DE])
    .reduce((res, pos) => res
      .fuse(makeCylinder(4.5, 8, pos).chamfer(1).translateZ(H-8))
      .cut(makeCylinder(2.5, 12, pos).translateZ(H-10))
    , frame);

  // N / S panel mount holes
  frame = [
    [3, -20,   1.5], [WD-3, -20,   1.5],
    [3, -20, H-1.5], [WD-3, -20, H-1.5],
    [3, -D,    1.5], [WD-3, -D,    1.5],
    [3, -D,  H-1.5], [WD-3, -D,  H-1.5],
  ]
    .reduce((res, pos) => res.cut(makeCylinder(1.3, 20, pos, [0, 180, 0])), frame);

  // T / B panel mount holes
  frame = [
    [3, -15,      0], [WD-3, -15,      0],
    [3, -15,   H-20], [WD-3, -15,   H-20],
    [3, -D/2,     0], [WD-3, -D/2,     0],
    [3, -D/2,  H-20], [WD-3, -D/2,  H-20],
    [3, -D+15,    0], [WD-3, -D+15,    0],
    [3, -D+15, H-20], [WD-3, -D+15, H-20],
  ]
    .reduce((res, pos) => res.cut(makeCylinder(1.3, 20, pos)), frame);

  return [
    {name: 'xapoh-case-frame-v1.0.0',  shape: frame,  color: '#aaa', opacity: 0.3},
    // {name: 'xapoh-case-xapoh-v1.0.0', shape: xapoh, color: '#eea', opacity: 0.5},
    // {name: 'xapoh-case-ekran-v1.0.0', shape: ekran, color: '#aee', opacity: 0.3},
    // {name: 'xapoh-case-SP-v1.0.0',  shape: SP,  color: '#aaa', opacity: 0.5},
    // {name: 'xapoh-case-NP-v1.0.0',  shape: NP,  color: '#aaa', opacity: 0.5}
  ];
};
