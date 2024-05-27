'use strict';

const jscad = require('@jscad/modeling');
const { colorize, hexToRgb } = jscad.colors;
const { polygon, cuboid, roundedCuboid, cylinder, cylinderElliptic, roundedCylinder } = jscad.primitives;
const { union, subtract, intersect } = jscad.booleans;
const { translate, translateX, translateY, translateZ, rotate, rotateX, rotateY, rotateZ } = jscad.transforms;
const { extrudeLinear, extrudeFromSlices, slice } = jscad.extrusions;
const { mat4 } = jscad.maths;
const { hull, hullChain } = jscad.hulls;

const outerKnob = () => subtract(
  cylinderElliptic({height: 21, startRadius: [12, 12], endRadius: [11, 11], center: [0, 0, 21 / 2 + 3], segments: 36}),
  cylinder({radius: 9, height: 2, center: [0, 0, 24], segments: 64}),
  cylinder({radius: 4.05,  height: 12, center: [0, 0, 20], segments: 64}),
  cylinderElliptic({height: 8, startRadius: [10, 10], endRadius: [2, 2], center: [0, 0, 13], segments: 64}),
  cylinderElliptic({height: 6, startRadius: [11, 11], endRadius: [10, 10], center: [0, 0, 6], segments: 64}),
  translateZ(19,
    rotateX(Math.PI / 2,
      cylinder({radius: 1, height: 40, center: [0, 0, 20]}),
    )
  ),
  ...Array.from({length: 18}, (e, i) => Math.PI * 2 * (i + 0.5) / 18)
    .map(angle => rotateZ(angle,
      translate([11.8, 0, 16],
        rotateY(-.05,
          roundedCylinder({height: 18, radius: 1, roundRadius: 0.5})
        )
      )
    ))
);

const outerInsert = () => translateZ(19,
  subtract(
    cylinder({radius: 4, height: 8}),
    cylinder({radius: 3, height: 8})
  )
);

const innnerKnob = () => translateZ(24, // 26
  subtract(
    cylinderElliptic({height: 14, startRadius: [8, 8], endRadius: [7.6, 7.6], center: [0, 0, 7], segments: 44}),
    cylinder({radius: 5.7 / 2, height: 11, center: [0, 0, 5], segments: 64}),
    translateZ(5,
      rotateX(Math.PI / 2,
        cylinder({radius: 1, height: 40, center: [0, 0, 20]}),
      )
    ),
    ...Array.from({length: 11}, (e, i) => Math.PI * 2 * (i + 0.25) / 11)
    .map(angle => rotateZ(angle,
      translate([8, 0, 10],
        rotateY(0,
          roundedCylinder({height: 15, radius: 1, roundRadius: 0.5})
        )
      )
    ))

  )
);

const innerInsert = () => translateZ(24,
  subtract(
    cylinder({radius: 5.7 / 2, height: 10, center: [0, 0, 5]}),
    cylinder({radius: 3.45 / 2, height: 10, center: [0, 0, 5]})
  )
);

const main = () => [
  colorize(hexToRgb('#aaaaff'), outerKnob()),
  colorize(hexToRgb('#ffccaa'), outerInsert()),
  colorize(hexToRgb('#ffaaaa'), innnerKnob()),
  colorize(hexToRgb('#ffccaa'), innerInsert()),
];

module.exports.main = main;
