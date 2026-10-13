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
