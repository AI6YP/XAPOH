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

  // Caption shown when the current text isn't valid JSON (so it won't save).
  const caption = document.createElement('div');
  caption.className = 'cfg-status';

  const validate = () => {
    const json = editor.innerText;
    try {
      JSON.parse(json);
      editor.style.backgroundColor = '#030';
      caption.style.display = 'none';
      localStorage.setItem('bridgeConfig', json);
    } catch (e) {
      editor.style.backgroundColor = '#300';
      caption.textContent = 'неверный JSON — не сохранено';
      caption.style.display = 'block';
    }
  };
  editor.addEventListener('input', validate);
  validate();

  const reset = document.createElement('div');
  reset.className = 'button cfg-reset';
  reset.setAttribute('role', 'button');
  reset.setAttribute('tabindex', '0');
  reset.textContent = 'Сбросить';
  const doReset = () => {
    editor.innerText = defaultBridgeConfig;
    localStorage.setItem('bridgeConfig', defaultBridgeConfig);
    validate();
  };
  reset.addEventListener('click', doReset);
  reset.addEventListener('keydown', (e) => {
    if (e.key === 'Enter' || e.key === ' ') {
      e.preventDefault();
      doReset();
    }
  });

  $root.appendChild(editor);
  $root.appendChild(caption);
  $root.appendChild(reset);

};
/* eslint-env browser */
