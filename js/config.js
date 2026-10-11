/* Etherial — site config. Fill in real links before release. */
window.EthConfig = {
  LAST_UPDATE: '11.10.26',
  /* Shared backend: paste your hosted API root here, e.g. 'https://etherial-api.onrender.com'.
     Empty = use same-origin /api (local server.py) or local demo on static hosting. */
  API_BASE: '',
  CONTACTS: {
    discord: '',   // e.g. 'https://discord.gg/etherial'
    telegram: '',  // e.g. 'https://t.me/etherial_client'
    youtube: ''    // e.g. 'https://youtube.com/@etherial'
  },
  /* Bypass status per server. state: 'work' = bypass ok, 'test' = on test, 'fix' = updating.
     Edit versions/dates here after each build. */
  BYPASSES: [
    { server: 'HolyWorld',  mode: 'Aura HW · Fly HW · Speed HW',  version: 'v1.4.2', updated: '10.10.26', state: 'work' },
    { server: 'FunTime',    mode: 'Aura FT · Fly FT · Speed FT',  version: 'v1.4.2', updated: '10.10.26', state: 'work' },
    { server: 'ReallyWorld', mode: 'Aura RW · Fly RW',           version: 'v1.4.1', updated: '08.10.26', state: 'work' },
    { server: 'DonutSMP',   mode: 'Aura DS · Fly DS',             version: 'v1.4.3b', updated: '11.10.26', state: 'test' }
  ]
};
