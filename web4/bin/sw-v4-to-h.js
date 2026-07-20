
export const swV4ToH = (data) => {
  let body = '';
  for (let i = 0; i < data.length; i++) {
    body += (i & 15) ? ' ' : '\n  ';
    body += '0x' + data[i].toString(16).padStart(2, '0');
    if (i < data.length - 1) { body += ','; }
  }
  return `\
/* CH32V003 expander firmware image (sw-v4/main.bin). Generated!! do not edit!! */
#pragma once
const unsigned int expander_bin_length = ${data.length};
const unsigned char expander_bin[] = {${body}
};
`;
};
