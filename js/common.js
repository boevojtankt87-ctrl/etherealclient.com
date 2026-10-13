/* Ethereal — common: theme settings, header, toasts */
(function () {
  'use strict';

  var THEME_ACC = { ethereal: '#4eba8e', mono: '#ededed', crimson: '#e0334f', toxic: '#b8ff29', royal: '#a678ff', arctic: '#0284c7' };
  function applyTheme() {
    var theme = localStorage.getItem('eth_theme') || 'ethereal';
    if (!THEME_ACC[theme]) theme = 'ethereal';
    var acc = localStorage.getItem('eth_accent') || THEME_ACC[theme];
    document.body.dataset.theme = theme;
    document.documentElement.style.setProperty('--acc', acc);
    document.documentElement.style.setProperty('--acc-dim', acc + '1f');
    document.body.classList.toggle('no-motion', localStorage.getItem('eth_motion') === 'off');
  }
  window.EthTheme = { apply: applyTheme };
  applyTheme();

  /* ---------- inline icon sprite (Phobia-style nav/card icons) ---------- */
  var SPRITE = '<svg xmlns="http://www.w3.org/2000/svg" style="display:none" aria-hidden="true">' +
    '<symbol id="i-home" viewBox="0 0 24 24"><path d="M4 11l8-7 8 7"/><path d="M6 10v9h12v-9"/></symbol>' +
    '<symbol id="i-bag" viewBox="0 0 24 24"><path d="M5 8h14l-1 12H6z"/><path d="M9 8V6a3 3 0 016 0v2"/></symbol>' +
    '<symbol id="i-tag" viewBox="0 0 24 24"><path d="M3 12V4h8l9 9-8 8z"/><circle cx="8" cy="8" r="1.4"/></symbol>' +
    '<symbol id="i-chat" viewBox="0 0 24 24"><path d="M4 6h16v9H9l-5 4z"/><circle cx="9" cy="11" r=".9"/><circle cx="13" cy="11" r=".9"/><circle cx="17" cy="11" r=".9"/></symbol>' +
    '<symbol id="i-user" viewBox="0 0 24 24"><circle cx="12" cy="8" r="3.5"/><path d="M5 20c0-3.5 3-5.5 7-5.5s7 2 7 5.5"/></symbol>' +
    '<symbol id="i-login" viewBox="0 0 24 24"><path d="M14 4h5v16h-5"/><path d="M10 8l4 4-4 4"/><path d="M14 12H3"/></symbol>' +
    '<symbol id="i-cart" viewBox="0 0 24 24"><path d="M3 4h2l2.4 11h10.2L20 7H6"/><circle cx="9" cy="19" r="1.5"/><circle cx="17" cy="19" r="1.5"/></symbol>' +
    '<symbol id="i-arrow" viewBox="0 0 24 24"><path d="M7 17L17 7"/><path d="M9 7h8v8"/></symbol>' +
    '<symbol id="i-arrow-r" viewBox="0 0 24 24"><path d="M4 12h15"/><path d="M13 6l6 6-6 6"/></symbol>' +
    '<symbol id="i-eye" viewBox="0 0 24 24"><path d="M2 12s3.5-6 10-6 10 6 10 6-3.5 6-10 6-10-6-10-6z"/><circle cx="12" cy="12" r="2.5"/></symbol>' +
    '<symbol id="i-lock" viewBox="0 0 24 24"><rect x="5" y="11" width="14" height="9" rx="2"/><path d="M8 11V7a4 4 0 018 0v4"/></symbol>' +
    '<symbol id="i-shield" viewBox="0 0 24 24"><path d="M12 3l7 3v5c0 5-3.5 8-7 10-3.5-2-7-5-7-10V6z"/><path d="M9 12l2 2 4-4"/></symbol>' +
    '<symbol id="i-bolt" viewBox="0 0 24 24"><path d="M13 2L5 14h6l-1 8 8-12h-6z"/></symbol>' +
    '<symbol id="i-clock" viewBox="0 0 24 24"><circle cx="12" cy="12" r="8"/><path d="M12 8v4l3 2"/></symbol>' +
    '<symbol id="i-crown" viewBox="0 0 24 24"><path d="M4 17h16"/><path d="M4 17l-1-9 5 4 4-6 4 6 5-4-1 9"/></symbol>' +
    '<symbol id="i-help" viewBox="0 0 24 24"><circle cx="12" cy="12" r="9"/><path d="M9.5 9a2.6 2.6 0 015 1c0 1.7-2.5 2-2.5 3.5"/><circle cx="12" cy="17" r=".6"/></symbol>' +
    '<symbol id="i-gear" viewBox="0 0 24 24"><circle cx="12" cy="12" r="3"/><path d="M12 2v3M12 19v3M2 12h3M19 12h3M4.9 4.9l2.1 2.1M17 17l2.1 2.1M19.1 4.9L17 7M7 17l-2.1 2.1"/></symbol>' +
    '<symbol id="i-download" viewBox="0 0 24 24"><path d="M12 4v10"/><path d="M8 11l4 4 4-4"/><path d="M4 19h16"/></symbol>' +
    '<symbol id="i-refresh" viewBox="0 0 24 24"><path d="M4 9a8 8 0 0114-3l2 2"/><path d="M20 15a8 8 0 01-14 3l-2-2"/><path d="M20 4v4h-4"/><path d="M4 20v-4h4"/></symbol>' +
    '<symbol id="i-card" viewBox="0 0 24 24"><rect x="3" y="6" width="18" height="13" rx="3"/><path d="M3 10h18M16 15h2"/></symbol>' +
    '<symbol id="i-discord" viewBox="0 0 24 24"><path d="M8 5.5S6.5 4 4.5 4C3 9 3 15 4.5 20c2 0 3.5-1.5 3.5-1.5"/><path d="M16 5.5S17.5 4 19.5 4C21 9 21 15 19.5 20c-2 0-3.5-1.5-3.5-1.5"/><path d="M8.5 17c2.3 1.2 4.7 1.2 7 0"/><circle cx="9.5" cy="12" r=".8"/><circle cx="14.5" cy="12" r=".8"/></symbol>' +
    '<symbol id="i-telegram" viewBox="0 0 24 24"><path d="M21 5L2.5 12l5.5 2 2 5.5L13 15l4.5 3.5L21 5z"/><path d="M8 14l10-8-6 10"/></symbol>' +
    '<symbol id="i-spark" viewBox="0 0 24 24"><path d="M12 3l1.8 5.2L19 10l-5.2 1.8L12 17l-1.8-5.2L5 10l5.2-1.8z"/><path d="M19 16l.8 2.2L22 19l-2.2.8L19 22l-.8-2.2L16 19l2.2-.8z"/></symbol>' +
    '</svg>';
  document.addEventListener('DOMContentLoaded', function () {
    if (!document.getElementById('eth-sprite')) {
      var wrap = document.createElement('div');
      wrap.id = 'eth-sprite';
      wrap.innerHTML = SPRITE;
      document.body.insertBefore(wrap, document.body.firstChild);
    }
    /* footer socials from EthConfig.CONTACTS */
    var fs = document.getElementById('footSocials');
    if (fs) {
      var C = (window.EthConfig && window.EthConfig.CONTACTS) || {};
      var soc = [
        ['discord', 'i-discord', 'Discord'],
        ['telegram', 'i-telegram', 'Telegram'],
        ['youtube', 'i-spark', 'YouTube']
      ].filter(function (s) { return C[s[0]]; }).map(function (s) {
        return '<a href="' + C[s[0]] + '" target="_blank" rel="noopener" aria-label="' + s[2] + '"><svg class="ico"><use href="#' + s[1] + '"></use></svg></a>';
      }).join('');
      if (soc) { fs.innerHTML = soc; fs.style.display = ''; } else { fs.style.display = 'none'; }
    }
    /* eye toggles on password fields */
    document.querySelectorAll('input[type="password"]').forEach(function (inp) {
      if (inp.closest('.remember') || inp.closest('.admin-sel')) return;
      var lab = inp.closest('label') || inp.parentElement;
      if (!lab) return;
      lab.classList.add('pw-wrap');
      var b = document.createElement('button');
      b.type = 'button';
      b.className = 'pw-eye';
      b.innerHTML = '<svg class="ico"><use href="#i-eye"></use></svg>';
      b.addEventListener('click', function () {
        var show = inp.type === 'password';
        inp.type = show ? 'text' : 'password';
        b.classList.toggle('on', show);
      });
      lab.appendChild(b);
    });
  });

  document.addEventListener('DOMContentLoaded', function () {
    if (window.EthStore && window.EthStore.mode) {
      window.EthStore.mode().then(function (m) {
        if (m !== 'local') return;
        var b = document.createElement('div');
        b.textContent = 'локальное демо: данные только в этом браузере';
        b.style.cssText = 'position:fixed;left:12px;bottom:12px;z-index:90;font-size:11px;opacity:.75;' +
          'background:var(--panel2,#111);border:1px solid var(--line,#333);color:var(--mut,#999);' +
          'padding:7px 12px;border-radius:10px;font-family:var(--font,system-ui)';
        document.body.appendChild(b);
      });
    }
    var burger = document.getElementById('burger');
    var nav = document.getElementById('nav');
    if (burger && nav) {
      burger.addEventListener('click', function () { nav.classList.toggle('open'); });
      nav.querySelectorAll('a').forEach(function (a) {
        a.addEventListener('click', function () { nav.classList.remove('open'); });
      });
    }
    // mark current page link
    var page = (location.pathname.split('/').pop() || 'index.html').toLowerCase();
    document.querySelectorAll('#nav a').forEach(function (a) {
      var href = (a.getAttribute('href') || '').toLowerCase();
      if (href.indexOf('cabinet') !== -1 && page.indexOf('cabinet') !== -1) a.classList.add('on');
    });
  });
})();

function toast(msg, isErr) {
  var box = document.getElementById('toasts');
  if (!box) return;
  var t = document.createElement('div');
  t.className = 'toast' + (isErr ? ' err' : '');
  t.textContent = msg;
  box.appendChild(t);
  setTimeout(function () {
    t.style.opacity = '0';
    setTimeout(function () { t.remove(); }, 300);
  }, 3400);
}

/* Compact SHA-256 (sync, no dependencies). Used to hash card PANs client-side. */
function sha256hex(str) {
  function rr(v, a) { return (v >>> a) | (v << (32 - a)); }
  var maxWord = Math.pow(2, 32), result = '';
  var words = [], bitLen = str.length * 8;
  var hash = [0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19];
  var k = [0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2];
  str += '\x80';
  while (str.length % 64 - 56) str += '\x00';
  for (var i = 0; i < str.length; i++) {
    var j = str.charCodeAt(i);
    if (j >> 8) return '';
    words[i >> 2] |= j << ((3 - i) % 4) * 8;
  }
  words[words.length] = (bitLen / maxWord) | 0;
  words[words.length] = bitLen;
  for (var k2 = 0; k2 < words.length;) {
    var w = words.slice(k2, k2 += 16);
    var a = hash[0], b = hash[1], c = hash[2], d = hash[3], e = hash[4], f = hash[5], g = hash[6], h = hash[7];
    for (var t = 0; t < 64; t++) {
      if (t < 16) { /* w[t] as is */ } else {
        var s0 = rr(w[t - 15], 7) ^ rr(w[t - 15], 18) ^ (w[t - 15] >>> 3);
        var s1 = rr(w[t - 2], 17) ^ rr(w[t - 2], 19) ^ (w[t - 2] >>> 10);
        w[t] = (w[t - 16] + s0 + w[t - 7] + s1) | 0;
      }
      var S1 = rr(e, 6) ^ rr(e, 11) ^ rr(e, 25);
      var ch = (e & f) ^ (~e & g);
      var t1 = (h + S1 + ch + k[t] + w[t]) | 0;
      var S0 = rr(a, 2) ^ rr(a, 13) ^ rr(a, 22);
      var maj = (a & b) ^ (a & c) ^ (b & c);
      var t2 = (S0 + maj) | 0;
      h = g; g = f; f = e; e = (d + t1) | 0; d = c; c = b; b = a; a = (t1 + t2) | 0;
    }
    hash[0] = (hash[0] + a) | 0; hash[1] = (hash[1] + b) | 0;
    hash[2] = (hash[2] + c) | 0; hash[3] = (hash[3] + d) | 0;
    hash[4] = (hash[4] + e) | 0; hash[5] = (hash[5] + f) | 0;
    hash[6] = (hash[6] + g) | 0; hash[7] = (hash[7] + h) | 0;
  }
  for (i = 0; i < hash.length; i++) {
    for (j = 3; j + 1; j--) {
      var b2 = (hash[i] >> (j * 8)) & 255;
      result += (b2 < 16 ? '0' : '') + b2.toString(16);
    }
  }
  return result;
}
