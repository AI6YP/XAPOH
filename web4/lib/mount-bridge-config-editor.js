const defaultBridgeConfig = `{ "wifi": { "ssid": "myssid", "password": "mypassword" } }`;

export const mountBridgeConfigEditor = ($root) => {

  // минималистический JSON-редактор на vanilla JS
  // contentediable + localStorage + автосохранение
  const editor = document.createElement('div');
  editor.contentEditable = true;
  editor.spellcheck = false;
  Object.assign(editor.style, {
    whiteSpace: 'pre',
    tabSize: 2,
    fontFamily: 'monospace',
    border: 'none',
    outline: 'none',
    borderRadius: '10px',
    backgroundColor: '#030',
    minHeight: '25px', // enough for one line
    fontSize: '14px',
    padding: '10px',
    color: '#fff'
  });
  editor.innerHTML = localStorage.getItem('bridgeConfig') || defaultBridgeConfig;
  editor.addEventListener('input', () => {
    const json = editor.innerText;
    console.log(json); // eslint-disable-line no-console
    try {
      JSON.parse(json);
      editor.style.backgroundColor = '#030';
      localStorage.setItem('bridgeConfig', json);
    } catch (e) {
      editor.style.backgroundColor = '#300';
    }
  });
  $root.appendChild(editor);

};
/* eslint-env browser */
