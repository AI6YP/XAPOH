'use strict';

const { makePolygon, makeSolid, makeBaseBox, makeCylinder, makeBox, draw } = replicad;

const defaultParams = {};

const main = (r, $) => {
  // XAPOH PCB
  const xapoh0 = [51.5, -25, 0];

  let xapoh = makeBox(xapoh0, [150, -110, 1]);

  xapoh = [
    [ 55.00,  -28.50], [ 96.00,  -29.00], [146.90,  -28.10],
    [ 56.00, -105.50], [ 99.05, -104.55], [145.55, -105.55],
    [112.37,  -48.09], [112.62,  -66.59], [112.62,  -57.09]
  ]
    // .map((p) => [p[0] - xapoh0[0], p[1] - xapoh0[1]])
    .reduce((res, point) => res.cut(makeCylinder(1.6, 2, point)), xapoh);

  xapoh = xapoh.fuse(makeCylinder((130.62 - 94.62) / 2, 12, [(130.62 + 94.62) / 2, -(75.08 + 39.09) / 2, -12]));

  xapoh = xapoh.translate(-xapoh0[0], -xapoh0[1], 20);

  // LEFT SIDE
  let left = makeBox([0, 0, 0], [100, 3, 30]);

  left = left.translate(0, 0.50, 0);

  // RIGHT SIDE
  let right = makeBox([0, 0, 0], [100, 3, 30]);

  right = right.translate(0, -85 - 3 - 0.50, 0);

  return [
    {name: 'xapoh-case-xapoh-v1.0.0', shape: xapoh, color: '#eea', opacity: 0.3},
    {name: 'xapoh-case-left-v1.0.0',  shape: left,  color: '#aaa', opacity: 0.3},
    {name: 'xapoh-case-right-v1.0.0', shape: right, color: '#aaa', opacity: 0.3},
  ];
};
