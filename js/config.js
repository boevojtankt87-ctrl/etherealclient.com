/* Ethereal — site config. Fill in real links before release. */
window.EthConfig = {
  LAST_UPDATE: '11.10.26',
  /* Shared backend: paste your hosted API root here, e.g. 'https://ethereal-api.onrender.com'.
     Empty = use same-origin /api (local server.py) or local demo on static hosting. */
  API_BASE: '',
  CONTACTS: {
    discord: '',   // e.g. 'https://discord.gg/ethereal'
    telegram: '',  // e.g. 'https://t.me/ethereal_client'
    youtube: ''    // e.g. 'https://youtube.com/@ethereal'
  },
  /* Bypass status per server. state: 'work' = bypass ok, 'test' = on test, 'fix' = updating.
     Edit versions/dates here after each build. */
  BYPASSES: [
    { server: 'HolyWorld',  mode: 'combat · render · пресеты',  version: 'v1.4.2', updated: '10.10.26', state: 'test' },
    { server: 'FunTime',    mode: 'combat · render · пресеты',  version: 'v1.4.2', updated: '10.10.26', state: 'test' },
    { server: 'ReallyWorld', mode: 'combat · render',           version: 'v1.4.1', updated: '08.10.26', state: 'test' },
    { server: 'DonutSMP',   mode: 'combat · render',             version: 'v1.4.3b', updated: '11.10.26', state: 'fix' }
  ]
};
