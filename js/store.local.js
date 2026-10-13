/* Ethereal — local backend (browser localStorage).
   Same async interface as the API backend. Used automatically when the
   Python server is unreachable (e.g. GitHub Pages static hosting).
   Demo-grade: data lives in this browser only. */
(function () {
  'use strict';

  var DB_KEY = 'eth_local_db';
  var DAY = 864e5;
  var PLANS = { TRIAL: 1, D7: 7, D30: 30, D90: 90, D180: 180 };
  var BUYABLE = { D7: 1, D30: 1, D90: 1, D180: 1 };
  var FEE = 10;

  function blank() {
    return { users: {}, counter: 1, keys: {}, cards: {}, sessions: {},
             chat: [], listings: [], seq: 1, purchases: [], txs: {},
             logs: {}, builds: null, polls: null, votes: [], hwidBans: {} };
  }
  function load() {
    try {
      var d = JSON.parse(localStorage.getItem(DB_KEY));
      if (d && d.users) {
        var dirty = false;
        for (var k in d.users) {
          var want = k === 'zxs' ? 'admin' : (d.users[k].role === 'admin' ? 'user' : d.users[k].role);
          if (d.users[k].role !== want) { d.users[k].role = want; dirty = true; }
        }
        if (dirty) save(d);
        return d;
      }
    } catch (e) {}
    return blank();
  }
  function save(d) { localStorage.setItem(DB_KEY, JSON.stringify(d)); }
  function ok(v) { return Promise.resolve(v); }

  function esc(s) {
    return String(s).replace(/[&<>"']/g, function (c) {
      return { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c];
    });
  }
  function rnd(n) {
    var abc = 'ABCDEFGHJKLMNPQRSTUVWXYZ23456789', s = '';
    for (var i = 0; i < n; i++) s += abc[Math.floor(Math.random() * abc.length)];
    return s;
  }
  function pwHash(nick, pass) {
    var h = (typeof sha256hex === 'function') ? sha256hex(nick + '|' + pass) : ('#' + nick + '#' + pass);
    return 'local$' + h;
  }
  function token() { return rnd(48); }
  function emailOk(e) { return /^[^@\s]+@[^@\s]+\.[^@\s]+$/.test(e || ''); }
  function fmtDate(ts) {
    var d = new Date(ts);
    function p(n) { return (n < 10 ? '0' : '') + n; }
    return p(d.getDate()) + '.' + p(d.getMonth() + 1) + '.' + String(d.getFullYear()).slice(2) +
           (arguments.length > 1 ? ' ' + p(d.getHours()) + ':' + p(d.getMinutes()) : '');
  }

  function seed(d) {
    if (!d.builds) {
      var now = Date.now();
      d.builds = [
        { version: 'v1.4.2', title: 'Обходы HolyWorld и FunTime', text: 'Переписаны ротации Aura и глайды ElytraFly. Пресеты лежат в облаке.', date: fmtDate(now - 3 * DAY) },
        { version: 'v1.0', title: 'Релиз Ethereal', text: 'Первый публичный билд: ClickGUI, Aura/Speed/ESP-пресеты, облачные конфиги, IRC.', date: fmtDate(now - 40 * DAY) }
      ];
    }
    if (!d.polls) {
      d.polls = [{ id: 1, question: 'Какой обход обновить следующим?',
                   options: ['DonutSMP Fly', 'ReallyWorld Speed', 'FunTime Aura', 'Новый TargetESP'],
                   active: true, ts: Date.now() - 2 * DAY }];
    }
    save(d);
  }

  function pub(u) {
    return { name: u.name, uid: u.uid, role: u.role, email: u.email, balance: u.balance || 0,
             hwid: u.hwid, created: u.created, subUntil: u.subUntil || 0, plan: u.plan || null,
             banned: u.banned || 0 };
  }
  function log(d, key, msg) {
    d.logs[key] = d.logs[key] || [];
    d.logs[key].push('<b>' + fmtDate(Date.now(), 1) + '</b> — ' + esc(msg));
    if (d.logs[key].length > 40) d.logs[key] = d.logs[key].slice(-40);
  }
  function tx(d, key, kind, amount, text) {
    d.txs[key] = d.txs[key] || [];
    d.txs[key].unshift({ date: fmtDate(Date.now(), 1), kind: kind, amount: amount, text: text });
    if (d.txs[key].length > 30) d.txs[key] = d.txs[key].slice(0, 30);
  }
  function meByToken(d, t) {
    var s = d.sessions[t];
    if (!s) return null;
    s.seen = Date.now();
    var u = d.users[s.key];
    if (!u) return null;
    u._key = s.key;
    return u;
  }

  var L = {
    register: function (nick, email, pass, remember) {
      var d = load(); seed(d);
      nick = (nick || '').trim();
      if (!/^[A-Za-z0-9_]{3,16}$/.test(nick)) return ok({ error: 'Ник: 3–16 символов (A-Z, 0-9, _)' });
      if (!pass || pass.length < 4) return ok({ error: 'Пароль: минимум 4 символа' });
      var key = nick.toLowerCase();
      if (d.users[key]) return ok({ error: 'Этот ник уже занят' });
      if (!emailOk(email)) return ok({ error: 'Введи корректную почту — она нужна для восстановления' });
      for (var k in d.users) {
        if ((d.users[k].email || '').toLowerCase() === (email || '').toLowerCase())
          return ok({ error: 'Эта почта уже привязана к другому аккаунту' });
      }
      var uid = d.counter++;
      d.users[key] = { name: nick, pass: pwHash(key, pass), uid: uid,
        role: key === 'zxs' ? 'admin' : 'user',
        email: email, hwid: '',
        created: Date.now(), subUntil: 0, plan: null, balance: 0 };
      var t = token();
      d.sessions[t] = { key: key, seen: Date.now() };
      log(d, key, 'аккаунт создан, UID ' + uid);
      save(d);
      return ok({ token: t, user: withLog(d, key) });
    },
    login: function (nick, pass) {
      var d = load(); seed(d);
      var u = d.users[(nick || '').trim().toLowerCase()];
      if (!u || u.pass !== pwHash((nick || '').trim().toLowerCase(), pass || ''))
        return ok({ error: 'Неверный ник или пароль' });
      var t = token();
      d.sessions[t] = { key: u.name.toLowerCase(), seen: Date.now() };
      log(d, u.name.toLowerCase(), 'вход в кабинет');
      save(d);
      return ok({ token: t, user: withLog(d, u.name.toLowerCase()) });
    },
    logout: function () { return ok({ ok: true }); },
    me: function (t) {
      var d = load(); seed(d);
      var u = meByToken(d, t);
      save(d);
      return ok(u ? withLog(d, u._key) : null);
    },
    activate: function (t, key) {
      var d = load();
      var u = meByToken(d, t);
      if (!u) return ok({ error: 'auth' });
      key = (key || '').trim().toUpperCase();
      if (!/^ETHR-[A-Z0-9]{4}-[A-Z0-9]{4}$/.test(key)) return ok({ error: 'Неверный формат. Пример: ETHR-AB12-CD34' });
      var k = d.keys[key];
      if (!k) return ok({ error: 'Ключ не найден. Проверь код или купи тариф.' });
      if (k.usedBy) return ok({ error: 'Этот ключ уже использован' });
      var days = PLANS[k.plan] || 30;
      u.subUntil = Math.max(Date.now(), u.subUntil || 0) + days * DAY;
      u.plan = k.plan;
      k.usedBy = u.uid;
      log(d, u._key, 'ключ ' + key + ' активирован → ' + k.plan + ' (+' + days + ' дн.)');
      save(d);
      return ok({ plan: k.plan, days: days });
    },
    buy: function (t, plan, cardHash, pay) {
      var d = load();
      var u = meByToken(d, t);
      if (!u) return ok({ error: 'auth' });
      if (!BUYABLE[plan]) return ok({ error: 'Нет такого тарифа' });
      var price = { D7: 100, D30: 300, D90: 550, D180: 800 }[plan];
      if ((u.balance || 0) < price)
        return ok({ error: 'Не хватает ' + (price - (u.balance || 0)) + ' ₽ на балансе. Пополни в кабинете.' });
      u.balance -= price;
      tx(d, u._key, 'sub_buy', -price, 'подписка ' + plan);
      var days = { D7: 7, D30: 30, D90: 90, D180: 180 }[plan];
      u.subUntil = Math.max(u.subUntil || 0, Date.now()) + days * DAY;
      u.plan = plan;
      log(d, u._key, 'подписка куплена с баланса: ' + plan + ' (+' + days + ' дн.)');
      save(d);
      return ok({ code: 'BAL-' + rnd(4) + '-' + rnd(4), plan: plan, days: days });
    },
    hwidReset: function (t) {
      var d = load();
      var u = meByToken(d, t);
      if (!u) return ok({ error: 'auth' });
      if (!u.hwid) return ok({ error: 'HWID ещё не привязан — сдавать нечего' });
      if ((u.balance || 0) < 150)
        return ok({ error: 'Не хватает ' + (150 - (u.balance || 0)) + ' ₽ на балансе. Пополни в кабинете.' });
      u.balance -= 150;
      tx(d, u._key, 'hwid_reset', -150, 'сброс HWID');
      u.hwid = '';
      log(d, u._key, 'привязка HWID сброшена (150₽ с баланса), привяжется при следующем запуске');
      save(d);
      return ok({ hwid: 'bound-on-next-launch' });
    },
    billing: function (t) {      var d = load();
      var u = meByToken(d, t);
      if (!u) return ok({ error: 'auth' });
      save(d);
      return ok({ balance: u.balance || 0, txs: d.txs[u._key] || [] });
    },
    topup: function (t, amount, cardHash) {
      var d = load();
      var u = meByToken(d, t);
      if (!u) return ok({ error: 'auth' });
      amount = parseInt(amount, 10);
      if (!amount || amount < 50 || amount > 100000) return ok({ error: 'Сумма от 50 до 100000 ₽' });
      u.balance = (u.balance || 0) + amount;
      tx(d, u._key, 'topup', amount, 'пополнение баланса');
      log(d, u._key, 'баланс пополнен на ' + amount + ' ₽');
      save(d);
      return ok({ balance: u.balance });
    },
    /* local demo: instantly activates, no real gateway */
    payCreate: function (t, kind, plan, amount) {
      var d = load();
      var u = meByToken(d, t);
      if (!u) return ok({ error: 'auth' });
      if (kind === 'topup') {
        amount = parseInt(amount, 10);
        if (!amount || amount < 50 || amount > 100000) return ok({ error: 'Сумма от 50 до 100000 ₽' });
        u.balance = (u.balance || 0) + amount;
        tx(d, u._key, 'topup', amount, 'пополнение баланса');
        log(d, u._key, 'баланс пополнен на ' + amount + ' ₽ (демо)');
        save(d);
        return ok({ url: '', balance: u.balance, demo: 1 });
      }
      if (kind === 'hwid') {
        if (!u.hwid) return ok({ error: 'HWID ещё не привязан — сдавать нечего' });
        if ((u.balance || 0) < 150) return ok({ error: 'Не хватает ' + (150 - (u.balance || 0)) + ' ₽ на балансе. Пополни в кабинете.' });
        u.balance -= 150;
        tx(d, u._key, 'hwid_reset', -150, 'сброс HWID');
        u.hwid = '';
        log(d, u._key, 'привязка HWID сброшена (150₽ с баланса)');
        save(d);
        return ok({ url: '', demo: 1 });
      }
      return ok({ error: 'Онлайн-оплата доступна только на боевом сервере. Пополни баланс и купи с него.' });
    },
    marketList: function (tag, q) {
      var d = load();
      var tags = ['HolyWorld', 'FunTime', 'ReallyWorld', 'DonutSMP', 'Universal'];
      var out = d.listings.filter(function (l) { return l.active; })
        .filter(function (l) { return !tag || l.tag === tag; })
        .filter(function (l) { return !q || l.title.toLowerCase().indexOf(q.toLowerCase()) !== -1; })
        .sort(function (a, b) { return b.id - a.id; }).slice(0, 60)
        .map(function (l) {
          var s = d.users[l.seller] || { name: '?', uid: 0 };
          return { id: l.id, title: l.title, tag: l.tag, price: l.price, sales: l.sales,
                   seller: s.name, sellerUid: s.uid, date: fmtDate(l.created) };
        });
      return ok({ listings: out, tags: tags });
    },
    marketCreate: function (t, f) {
      var d = load();
      var u = meByToken(d, t);
      if (!u) return ok({ error: 'auth' });
      var title = (f.title || '').trim(), descr = (f.descr || '').trim(), cfg = (f.config || '').trim();
      var price = parseInt(f.price, 10);
      var tags = ['HolyWorld', 'FunTime', 'ReallyWorld', 'DonutSMP', 'Universal'];
      if (!(title.length >= 4 && title.length <= 80)) return ok({ error: 'Название: 4–80 символов' });
      if (tags.indexOf(f.tag) === -1) return ok({ error: 'Выбери сервер из списка' });
      if (!price || price < 10 || price > 100000) return ok({ error: 'Цена: 10–100000 ₽' });
      if (!(descr.length >= 10 && descr.length <= 2000)) return ok({ error: 'Описание: 10–2000 символов' });
      if (!(cfg.length >= 10 && cfg.length <= 20000)) return ok({ error: 'Вставь текст конфига (10–20000 символов)' });
      var id = d.seq++;
      d.listings.push({ id: id, seller: u._key, title: title, tag: f.tag, price: price,
                        descr: descr, config: cfg, created: Date.now(), active: true, sales: 0 });
      log(d, u._key, 'лот #' + id + ' выставлен на маркет (' + price + ' ₽)');
      save(d);
      return ok({ id: id });
    },
    marketBuy: function (t, id) {
      var d = load();
      var u = meByToken(d, t);
      if (!u) return ok({ error: 'auth' });
      var l = null;
      d.listings.forEach(function (x) { if (x.id === +id) l = x; });
      if (!l || !l.active) return ok({ error: 'Лот снят с продажи' });
      if (l.seller === u._key) return ok({ error: 'Это твой собственный лот' });
      var owned = d.purchases.some(function (p) { return p.buyer === u._key && p.lot === +id; });
      if (owned) return ok({ error: 'Ты уже купил этот конфиг — он во вкладке «Купленные»' });
      if ((u.balance || 0) < l.price)
        return ok({ error: 'Не хватает ' + (l.price - (u.balance || 0)) + ' ₽ на балансе. Пополни в кабинете.' });
      var cut = Math.floor(l.price * (100 - FEE) / 100);
      u.balance -= l.price;
      d.users[l.seller].balance = (d.users[l.seller].balance || 0) + cut;
      d.purchases.push({ buyer: u._key, lot: +id, price: l.price, ts: Date.now() });
      l.sales++;
      tx(d, u._key, 'market_buy', -l.price, 'конфиг «' + l.title.slice(0, 40) + '»');
      tx(d, l.seller, 'market_sale', cut, 'продажа «' + l.title.slice(0, 40) + '» (комиссия ' + FEE + '%)');
      log(d, u._key, 'куплен конфиг «' + l.title.slice(0, 40) + '» за ' + l.price + ' ₽');
      log(d, l.seller, 'продан конфиг «' + l.title.slice(0, 40) + '» (+' + cut + ' ₽)');
      save(d);
      return ok({ title: l.title, config: l.config, seller: d.users[l.seller].name });
    },
    marketMy: function (t) {
      var d = load();
      var u = meByToken(d, t);
      if (!u) return ok({ error: 'auth' });
      save(d);
      return ok({
        mine: d.listings.filter(function (l) { return l.seller === u._key; })
          .map(function (l) { return { id: l.id, title: l.title, tag: l.tag, price: l.price,
            sales: l.sales, active: l.active, date: fmtDate(l.created) }; }),
        bought: d.purchases.filter(function (p) { return p.buyer === u._key; })
          .map(function (p) {
            var l = {}, s = { name: '?' };
            d.listings.forEach(function (x) { if (x.id === p.lot) l = x; });
            if (d.users[l.seller]) s = d.users[l.seller];
            return { id: p.lot, title: l.title, tag: l.tag, price: p.price,
                     seller: s.name, date: fmtDate(p.ts) };
          })
      });
    },
    marketConfig: function (t, id) {
      var d = load();
      var u = meByToken(d, t);
      if (!u) return ok({ error: 'auth' });
      var owned = d.purchases.some(function (p) { return p.buyer === u._key && p.lot === +id; });
      var l = null;
      d.listings.forEach(function (x) { if (x.id === +id) l = x; });
      if (!l || (!owned && l.seller !== u._key)) return ok({ error: 'Сначала купи этот конфиг' });
      return ok({ title: l.title, tag: l.tag, config: l.config });
    },
    marketToggle: function (t, id, active) {
      var d = load();
      var u = meByToken(d, t);
      if (!u) return ok({ error: 'auth' });
      var l = null;
      d.listings.forEach(function (x) { if (x.id === +id) l = x; });
      if (!l || l.seller !== u._key) return ok({ error: 'Не твой лот' });
      l.active = !!active;
      log(d, u._key, 'лот #' + id + ' ' + (active ? 'возвращён на витрину' : 'снят с продажи'));
      save(d);
      return ok({ ok: true });
    },
    chat: function () {
      var d = load(); seed(d);
      return ok(d.chat.slice(-60));
    },
    post: function (t, text) {
      var d = load();
      var u = meByToken(d, t);
      if (!u) return ok({ error: 'auth' });
      text = (text || '').trim().slice(0, 500);
      if (!text) return ok({ error: 'Пустое сообщение' });
      d.chat.push({ n: u.name, uid: u.uid, role: u.role, t: Date.now(), text: text });
      if (d.chat.length > 200) d.chat = d.chat.slice(-200);
      save(d);
      return ok({ ok: true });
    },
    builds: function () {
      var d = load(); seed(d);
      return ok(d.builds);
    },
    poll: function (t) {
      var d = load(); seed(d);
      var p = null;
      d.polls.forEach(function (x) { if (x.active) p = x; });
      if (!p) return ok({ poll: null });
      var counts = p.options.map(function () { return 0; });
      d.votes.forEach(function (v) {
        if (v.poll === p.id && counts[v.opt] !== undefined) counts[v.opt]++;
      });
      var my = null, u = t ? meByToken(d, t) : null;
      if (u) d.votes.forEach(function (v) {
        if (v.poll === p.id && v.user === u._key) my = v.opt;
      });
      save(d);
      return ok({ poll: { id: p.id, question: p.question, options: p.options,
        counts: counts, total: counts.reduce(function (a, b) { return a + b; }, 0), my: my } });
    },
    vote: function (t, opt) {
      var d = load();
      var u = meByToken(d, t);
      if (!u) return ok({ error: 'Войди, чтобы голосовать' });
      var p = null;
      d.polls.forEach(function (x) { if (x.active) p = x; });
      if (!p) return ok({ error: 'Нет активного опроса' });
      opt = +opt;
      if (!(opt >= 0 && opt < p.options.length)) return ok({ error: 'Нет такого варианта' });
      var dup = d.votes.some(function (v) { return v.poll === p.id && v.user === u._key; });
      if (dup) return ok({ error: 'Ты уже голосовал в этом опросе' });
      d.votes.push({ poll: p.id, user: u._key, opt: opt });
      save(d);
      return ok({ ok: true });
    },
    adminOverview: function (t) {
      var d = load();
      var u = meByToken(d, t);
      if (!u || u.role !== 'admin') return ok({ error: 'Только для админа' });
      save(d);
      var list = Object.keys(d.users).map(function (k) { return d.users[k]; })
        .sort(function (a, b) { return a.uid - b.uid; })
        .map(function (x) { return { key: x.name.toLowerCase(), name: x.name, uid: x.uid,
          role: x.role, plan: x.plan, leftMs: Math.max(0, (x.subUntil || 0) - Date.now()) }; });
      var unused = Object.keys(d.keys).filter(function (k) { return !d.keys[k].usedBy; }).length;
      var active = list.filter(function (x) { return x.leftMs > 0; }).length;
      return ok({ total: list.length, active: active, unusedKeys: unused, users: list });
    },
    adminKey: function (t, plan) {
      var d = load();
      var u = meByToken(d, t);
      if (!u || u.role !== 'admin') return ok({ error: 'Только для админа' });
      if (!BUYABLE[plan]) return ok({ error: 'Нет такого тарифа' });
      var code = 'ETHR-' + rnd(4) + '-' + rnd(4);
      d.keys[code] = { plan: plan, created: Date.now(), usedBy: null };
      save(d);
      return ok({ code: code });
    },
    adminRole: function (t, nick, role) {
      var d = load();
      var u = meByToken(d, t);
      if (!u || u.role !== 'admin') return ok({ error: 'Только для админа' });
      var roles = ['user', 'tester', 'youtube', 'tiktok', 'moderator', 'admin'];
      if (roles.indexOf(role) === -1) return ok({ error: 'Неизвестная роль' });
      var x = d.users[(nick || '').toLowerCase()];
      if (!x) return ok({ error: 'Пользователь не найден' });
      if (x.name.toLowerCase() === u.name.toLowerCase() && role !== 'admin')
        return ok({ error: 'Нельзя снять роль с себя' });
      if (role === 'admin' && x.name.toLowerCase() !== 'zxs')
        return ok({ error: 'Админ может быть только zxs' });
      x.role = role;
      save(d);
      return ok({ ok: true });
    },
    adminDelete: function (t, nick) {
      var d = load();
      var u = meByToken(d, t);
      if (!u || u.role !== 'admin') return ok({ error: 'Только для админа' });
      var key = (nick || '').toLowerCase();
      if (!d.users[key]) return ok({ error: 'Пользователь не найден' });
      if (key === u.name.toLowerCase()) return ok({ error: 'Нельзя удалить себя' });
      delete d.users[key];
      for (var s in d.sessions) if (d.sessions[s].key === key) delete d.sessions[s];
      save(d);
      return ok({ ok: true });
    },
    adminBuild: function (t, v, title, text) {
      var d = load(); seed(d);
      var u = meByToken(d, t);
      if (!u || u.role !== 'admin') return ok({ error: 'Только для админа' });
      v = (v || '').trim(); title = (title || '').trim(); text = (text || '').trim();
      if (!v || !title || !text) return ok({ error: 'Заполни версию, заголовок и текст' });
      d.builds.unshift({ version: v.slice(0, 16), title: title.slice(0, 120),
                         text: text.slice(0, 2000), date: fmtDate(Date.now()) });
      save(d);
      return ok({ ok: true });
    },
    adminUsers: function (t) {
      var d = load();
      var u = meByToken(d, t);
      if (!u || u.role !== 'admin') return ok({ error: 'Только для админа' });
      d.hwidBans = d.hwidBans || {};
      var now = Date.now(); var active = 0; var users = [];
      for (var k in d.users) {
        var x = d.users[k];
        var left = Math.max(0, (x.subUntil || 0) - now);
        if (left > 0) active++;
        users.push({ id: x.uid, uid: x.uid, name: x.name, key: k, email: x.email || '', role: x.role,
                     plan: x.plan || null, subUntil: x.subUntil || 0, leftMs: left,
                     hwid: x.hwid || '', balance: x.balance || 0, banned: x.banned || 0,
                     created: x.created });
      }
      users.sort(function (a, b) { return a.uid - b.uid; });
      var hwids = [];
      for (var h in d.hwidBans) {
        hwids.push({ hwid: h, by: d.hwidBans[h].by || 'admin', note: d.hwidBans[h].note || '',
                     date: fmtDate(d.hwidBans[h].ts || now) });
      }
      var unused = 0;
      for (var c in d.keys) if (!d.keys[c].used) unused++;
      return ok({ total: users.length, active: active, unusedKeys: unused, users: users, hwids: hwids });
    },
    adminFind: function (t, nick) {
      var d = load();
      var u = meByToken(d, t);
      if (!u || u.role !== 'admin') return ok({ error: 'Только для админа' });
      var x = d.users[(nick || '').toLowerCase()];
      if (!x) return ok({ error: 'Пользователь не найден' });
      return ok({ user: pub(x) });
    },
    adminBan: function (t, nick, banned) {
      var d = load();
      var u = meByToken(d, t);
      if (!u || u.role !== 'admin') return ok({ error: 'Только для админа' });
      var key = (nick || '').toLowerCase();
      var x = d.users[key];
      if (!x) return ok({ error: 'Пользователь не найден' });
      if (key === u.name.toLowerCase()) return ok({ error: 'Нельзя забанить себя' });
      if (banned && x.role === 'admin') return ok({ error: 'Нельзя забанить админа' });
      x.banned = banned ? 1 : 0;
      if (banned) for (var s in d.sessions) if (d.sessions[s].key === key) delete d.sessions[s];
      log(d, key, banned ? 'аккаунт заблокирован (' + u.name + ')' : 'аккаунт разблокирован (' + u.name + ')');
      save(d);
      return ok({ ok: true, banned: x.banned });
    },
    adminBanHwid: function (t, hwid, banned) {
      var d = load();
      var u = meByToken(d, t);
      if (!u || u.role !== 'admin') return ok({ error: 'Только для админа' });
      hwid = String(hwid || '').toUpperCase().trim();
      if (!/^[A-Z0-9]{4}-[A-Z0-9]{4}-[A-Z0-9]{4}$/.test(hwid))
        return ok({ error: 'Формат HWID: XXXX-XXXX-XXXX' });
      d.hwidBans = d.hwidBans || {};
      if (banned) d.hwidBans[hwid] = { by: u.name, ts: Date.now() };
      else delete d.hwidBans[hwid];
      save(d);
      return ok({ ok: true, banned: banned ? 1 : 0 });
    },
    adminSub: function (t, nick, days) {
      var d = load();
      var u = meByToken(d, t);
      if (!u || u.role !== 'admin') return ok({ error: 'Только для админа' });
      days = days | 0;
      if (days < 0 || days > 3650) return ok({ error: 'Дни: от 0 до 3650 (0 = снять подписку)' });
      var key = (nick || '').toLowerCase();
      var x = d.users[key];
      if (!x) return ok({ error: 'Пользователь не найден' });
      if (days === 0) {
        x.subUntil = Date.now(); x.plan = null;
        log(d, key, 'подписка снята (' + u.name + ')');
        save(d);
        return ok({ ok: true, subUntil: x.subUntil, plan: null });
      }
      var base = Math.max(Date.now(), x.subUntil || 0);
      x.subUntil = base + days * DAY;
      x.plan = x.plan || 'D30';
      log(d, key, 'подписка продлена +' + days + ' дн. (' + u.name + ')');
      save(d);
      return ok({ ok: true, subUntil: x.subUntil, plan: x.plan });
    },
    adminBalance: function (t, nick, amount) {
      var d = load();
      var u = meByToken(d, t);
      if (!u || u.role !== 'admin') return ok({ error: 'Только для админа' });
      amount = amount | 0;
      if (!amount || amount < -1000000 || amount > 1000000)
        return ok({ error: 'Сумма: от -1000000 до 1000000, не 0' });
      var key = (nick || '').toLowerCase();
      var x = d.users[key];
      if (!x) return ok({ error: 'Пользователь не найден' });
      x.balance = (x.balance || 0) + amount;
      tx(d, key, 'admin_adjust', amount, 'корректировка баланса (' + u.name + ')');
      log(d, key, 'баланс ' + (amount > 0 ? '+' : '') + amount + ' ₽ (' + u.name + ')');
      save(d);
      return ok({ ok: true, balance: x.balance });
    },
    adminHwidReset: function (t, nick) {
      var d = load();
      var u = meByToken(d, t);
      if (!u || u.role !== 'admin') return ok({ error: 'Только для админа' });
      var key = (nick || '').toLowerCase();
      var x = d.users[key];
      if (!x) return ok({ error: 'Пользователь не найден' });
      x.hwid = '';
      log(d, key, 'привязка HWID сброшена администратором (' + u.name + ')');
      save(d);
      return ok({ ok: true });
    },
    adminPoll: function (t, q, opts) {
      var d = load(); seed(d);
      var u = meByToken(d, t);
      if (!u || u.role !== 'admin') return ok({ error: 'Только для админа' });
      q = (q || '').trim();
      var arr = (opts || '').split(',').map(function (s) { return s.trim(); })
        .filter(Boolean).slice(0, 5);
      if (!q || arr.length < 2) return ok({ error: 'Нужен вопрос и минимум 2 варианта через запятую' });
      d.polls.forEach(function (p) { p.active = false; });
      var id = 1;
      d.polls.forEach(function (p) { id = Math.max(id, p.id + 1); });
      d.polls.push({ id: id, question: q.slice(0, 160), options: arr, active: true, ts: Date.now() });
      save(d);
      return ok({ ok: true });
    },
    stats: function () {
      var d = load();
      var total = Object.keys(d.users).length;
      var seen = {};
      var now = Date.now();
      for (var s in d.sessions) {
        if (now - d.sessions[s].seen < 900000) seen[d.sessions[s].key] = 1;
      }
      return ok({ users: total, online: Object.keys(seen).length });
    }
  };

  function bindCard(d, u, hash) {
    if (!hash || hash.length !== 64 || !/^[0-9a-f]+$/.test(hash)) return 'Введи данные карты';
    var key = u.name.toLowerCase();
    for (var h in d.cards) {
      if (h === hash && d.cards[h] !== key) {
        var uid = d.users[d.cards[h]] ? d.users[d.cards[h]].uid : '?';
        return 'Эта карта уже привязана к UID #' + uid + '. Одна карта — один аккаунт.';
      }
    }
    if (!d.cards[hash]) {
      d.cards[hash] = key;
      log(d, key, 'привязана карта •• ' + hash.slice(0, 6));
    }
    return null;
  }

  function withLog(d, key) {
    var u = pub(d.users[key]);
    u.log = d.logs[key] || [];
    return u;
  }

  window.EthStoreLocal = L;
})();
