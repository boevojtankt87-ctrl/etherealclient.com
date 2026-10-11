/* Etherial — store facade.
   Tries the Python API first; if unreachable (static hosting like
   GitHub Pages) falls back to the local browser backend.
   All methods return promises with identical shapes. */
(function () {
  'use strict';

  var DAY = 864e5;
  var PLANS = { TRIAL: 1, D7: 7, D30: 30, D90: 90, D180: 180 };
  var ROLES = ['user', 'tester', 'youtube', 'tiktok', 'moderator', 'admin'];

  function esc(s) {
    return String(s).replace(/[&<>"']/g, function (c) {
      return { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c];
    });
  }

  /* token lives in either storage (remember me or not) */
  function token() { return localStorage.getItem('eth_token') || sessionStorage.getItem('eth_token'); }
  function saveToken(t, remember) {
    localStorage.removeItem('eth_token');
    sessionStorage.removeItem('eth_token');
    (remember !== false ? localStorage : sessionStorage).setItem('eth_token', t);
  }
  function clearToken() {
    localStorage.removeItem('eth_token');
    sessionStorage.removeItem('eth_token');
  }

  /* ---------- API backend ---------- */
  function apiFetch(path, data) {
    var headers = { 'Content-Type': 'application/json' };
    var t = token();
    if (t) headers['Authorization'] = 'Bearer ' + t;
    return fetch(path, {
      method: data === undefined ? 'GET' : 'POST',
      headers: headers,
      body: data === undefined ? undefined : JSON.stringify(data)
    }).then(function (r) { return r.json(); });
  }

  var ApiBackend = {
    register: function (nick, email, pass, rem, ref) {
      return apiFetch('/api/register', { nick: nick, email: email, pass: pass, ref: ref || '' });
    },
    login: function (nick, pass) {
      return apiFetch('/api/login', { nick: nick, pass: pass });
    },
    logout: function () { return apiFetch('/api/logout', {}); },
    me: function () {
      if (!token()) return Promise.resolve(null);
      return apiFetch('/api/me').then(function (r) { return r.user || null; });
    },
    activate: function (t, key) { return apiFetch('/api/activate', { key: key }); },
    buy: function (t, plan, hash, pay) {
      return apiFetch('/api/buy', { plan: plan, card_hash: hash || '', pay: pay || 'card' });
    },
    billing: function () { return apiFetch('/api/billing', {}); },
    topup: function (t, amount, hash) { return apiFetch('/api/topup', { amount: amount, card_hash: hash }); },
    marketList: function (tag, q) {
      return fetch('/api/market?tag=' + encodeURIComponent(tag || '') + '&q=' + encodeURIComponent(q || ''))
        .then(function (r) { return r.json(); });
    },
    marketCreate: function (t, d) { return apiFetch('/api/market/create', d); },
    marketBuy: function (t, id) { return apiFetch('/api/market/buy', { id: id }); },
    marketMy: function () { return apiFetch('/api/market/my', {}); },
    marketConfig: function (t, id) { return apiFetch('/api/market/config', { id: id }); },
    marketToggle: function (t, id, active) { return apiFetch('/api/market/toggle', { id: id, active: active }); },
    chat: function () { return apiFetch('/api/chat').then(function (r) { return r.messages || []; }); },
    post: function (t, text) { return apiFetch('/api/chat', { text: text }); },
    refStats: function () { return apiFetch('/api/ref', {}); },
    builds: function () { return apiFetch('/api/builds').then(function (r) { return r.builds || []; }); },
    poll: function () { return apiFetch('/api/poll'); },
    vote: function (t, opt) { return apiFetch('/api/poll/vote', { option: opt }); },
    adminOverview: function () { return apiFetch('/api/admin/overview'); },
    adminKey: function (t, plan) { return apiFetch('/api/admin/key', { plan: plan }); },
    adminRole: function (t, nick, role) { return apiFetch('/api/admin/role', { nick: nick, role: role }); },
    adminDelete: function (t, nick) { return apiFetch('/api/admin/delete', { nick: nick }); },
    adminBuild: function (t, v, title, text) {
      return apiFetch('/api/admin/build', { version: v, title: title, text: text });
    },
    adminPoll: function (t, q, opts) { return apiFetch('/api/admin/poll', { question: q, options: opts }); },
    stats: function () {
      return apiFetch('/api/stats').then(function (r) {
        return { users: r.users || 0, online: r.online || 0 };
      });
    }
  };

  /* ---------- probe + facade ---------- */
  function probe() {
    try {
      return fetch('/api/stats', { cache: 'no-store' })
        .then(function (r) {
          if (!r.ok) throw 0;
          return r.json();
        })
        .then(function () { return 'api'; })
        .catch(function () { return 'local'; });
    } catch (e) {
      return Promise.resolve('local');
    }
  }
  var backendReady = probe().then(function (mode) {
    return mode === 'api' ? ApiBackend : window.EthStoreLocal;
  });

  function call(name, args, after) {
    return backendReady.then(function (b) {
      return b[name].apply(b, args);
    }).then(function (r) {
      if (after) after(r);
      return r;
    }).catch(function () {
      /* API died mid-session -> retry once against local */
      if (backendReady.__mode !== 'local') {
        backendReady = Promise.resolve(window.EthStoreLocal);
        backendReady.__mode = 'local';
        return window.EthStoreLocal[name].apply(window.EthStoreLocal, args);
      }
      return { error: 'Нет связи. Обнови страницу' };
    });
  }

  var S = {
    PLANS: PLANS,
    ROLES: ROLES,
    PRICES: { D7: 100, D30: 300, D90: 550, D180: 800, HWID: 150 },
    PLAN_LABEL: { TRIAL: 'тест 24ч', BONUS: 'бонус', D7: '7 дней', D30: '30 дней', D90: '90 дней', D180: '180 дней' },
    esc: esc,
    hasSession: function () { return !!token(); },
    mode: function () { return backendReady.then(function (b) { return b === ApiBackend ? 'api' : 'local'; }); },
    subLeft: function (u) { return Math.max(0, (u.subUntil || 0) - Date.now()); },
    fmtLeft: function (ms) {
      var d = Math.floor(ms / DAY), h = Math.floor((ms % DAY) / 36e5);
      if (d > 0) return d + ' дн. ' + h + ' ч.';
      if (h > 0) return h + ' ч.';
      return 'меньше часа';
    }
  };

  /* every method: first arg is always the session token ('' = use stored) */
  function T(args) {
    var a = Array.prototype.slice.call(args);
    a.unshift(token() || '');
    return a;
  }
  S.register = function (nick, email, pass, remember, ref) {
    return call('register', [nick, email, pass, remember, ref]).then(function (r) {
      if (r && r.token) saveToken(r.token, remember);
      return r;
    });
  };
  S.login = function (nick, pass, remember) {
    return call('login', [nick, pass]).then(function (r) {
      if (r && r.token) saveToken(r.token, remember);
      return r;
    });
  };
  S.logout = function () {
    return call('logout', []).then(function (r) { clearToken(); return r; });
  };
  ['me', 'activate', 'buy', 'billing', 'topup',
   'marketCreate',
   'marketBuy', 'marketMy', 'marketConfig', 'marketToggle', 'chat', 'post',
   'refStats', 'poll', 'vote', 'adminOverview', 'adminKey', 'adminRole',
   'adminDelete', 'adminBuild', 'adminPoll'
  ].forEach(function (name) {
    S[name] = function () { return call(name, T(arguments)); };
  });
  /* no session token needed */
  S.marketList = function (tag, q) { return call('marketList', [tag, q]); };
  S.builds = function () { return call('builds', []); };
  S.stats = function () { return call('stats', []); };

  window.EthStore = S;
})();
