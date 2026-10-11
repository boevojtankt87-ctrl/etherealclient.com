/* Etherial — shared card helpers (validation client-side, only SHA-256 goes to server) */
(function () {
  'use strict';

  function digits(el) { return (el.value || '').replace(/\D/g, ''); }

  function luhnOk(num) {
    if (!/^\d{16}$/.test(num)) return false;
    var s = 0, dbl = false;
    for (var i = num.length - 1; i >= 0; i--) {
      var n = +num[i];
      if (dbl) { n *= 2; if (n > 9) n -= 9; }
      s += n; dbl = !dbl;
    }
    return s % 10 === 0;
  }

  window.PayCard = {
    digits: digits,
    luhnOk: luhnOk,
    wireNum: function (el) {
      el.addEventListener('input', function () {
        var d = el.value.replace(/\D/g, '').slice(0, 16);
        el.value = d.replace(/(\d{4})(?=\d)/g, '$1 ');
      });
    },
    wireExp: function (el) {
      el.addEventListener('input', function () {
        var d = el.value.replace(/\D/g, '').slice(0, 4);
        el.value = d.length > 2 ? d.slice(0, 2) + '/' + d.slice(2) : d;
      });
    },
    wireCvc: function (el) {
      el.addEventListener('input', function () {
        el.value = el.value.replace(/\D/g, '').slice(0, 3);
      });
    },
    /* returns {ok:true, hash} or {ok:false, err} */
    validate: function (numEl, expEl, cvcEl) {
      var num = digits(numEl);
      if (!luhnOk(num)) return { ok: false, err: 'Проверь номер карты' };
      var m = (expEl.value || '').match(/^(\d{2})\/(\d{2})$/);
      if (!m || +m[1] < 1 || +m[1] > 12) return { ok: false, err: 'Срок в формате ММ/ГГ' };
      if (new Date(2000 + (+m[2]), +m[1]) <= new Date()) return { ok: false, err: 'Срок карты истёк' };
      if (!/^\d{3}$/.test(cvcEl.value || '')) return { ok: false, err: 'CVC — 3 цифры' };
      if (typeof sha256hex !== 'function') return { ok: false, err: 'Нет связи. Обнови страницу' };
      return { ok: true, hash: sha256hex(num) };
    }
  };
})();
