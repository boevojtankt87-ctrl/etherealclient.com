/* Etherial — landing page logic */
(function () {
  'use strict';
  var S = window.EthStore;
  var CFG = window.EthConfig || { CONTACTS: {} };

  /* static background grid */
  (function () {
    var c = document.getElementById('bg');
    if (!c) return;
    var x = c.getContext('2d'), W, H;
    function rs() { W = c.width = innerWidth; H = c.height = innerHeight; }
    rs(); addEventListener('resize', rs);
    x.clearRect(0, 0, W, H);
    var acc = getComputedStyle(document.documentElement).getPropertyValue('--acc').trim() || '#4eba8e';
    x.strokeStyle = acc + '12';
    x.lineWidth = 1;
    var step = 72;
    x.beginPath();
    for (var gx = 0; gx < W + step; gx += step) { x.moveTo(gx, 0); x.lineTo(gx, H); }
    for (var gy = 0; gy < H + step; gy += step) { x.moveTo(0, gy); x.lineTo(W, gy); }
    x.stroke();
  })();

  /* intro animation (friend's frames), once per session */
  (function () {
    var intro = document.getElementById('intro');
    if (!intro) return;
    function finish() {
      intro.classList.add('done');
      try { sessionStorage.setItem('eth_intro', '1'); } catch (e) {}
      setTimeout(function () { intro.classList.add('gone'); }, 850);
    }
    var seen = false;
    try { seen = sessionStorage.getItem('eth_intro') === '1'; } catch (e) {}
    if (seen || localStorage.getItem('eth_motion') === 'off') {
      intro.classList.add('gone');
      return;
    }
    intro.classList.add('play');
    intro.addEventListener('click', finish);
    setTimeout(finish, 3900);
  })();

  /* reveal */
  var io = new IntersectionObserver(function (es) {
    es.forEach(function (e) { if (e.isIntersecting) e.target.classList.add('vis'); });
  }, { threshold: 0.1 });
  document.querySelectorAll('.reveal').forEach(function (el) { io.observe(el); });

  /* live stats from the database */
  function paintStats() {
    S.stats().then(function (s) {
      var a = document.getElementById('statUsers');
      if (a) a.textContent = (s.users || 0).toLocaleString('ru-RU');
      var b = document.getElementById('statOnline');
      if (b) b.textContent = s.online || 0;
    });
  }
  paintStats();
  setInterval(paintStats, 15000);
  var lu = document.getElementById('statUpdate');
  if (lu && CFG.LAST_UPDATE) lu.textContent = CFG.LAST_UPDATE;

  /* faq */
  document.querySelectorAll('.qa').forEach(function (qa) {
    var body = qa.querySelector('.qa-body');
    body.innerHTML = '<div class="qa-body-inner">' + body.innerHTML + '</div>';
    var inner = qa.querySelector('.qa-body-inner');
    qa.querySelector('button').addEventListener('click', function () {
      var open = qa.classList.contains('open');
      document.querySelectorAll('.qa').forEach(function (x) {
        x.classList.remove('open');
        x.querySelector('.qa-body').style.maxHeight = null;
      });
      if (!open) { qa.classList.add('open'); body.style.maxHeight = inner.scrollHeight + 'px'; }
    });
  });

  /* store tabs */
  document.querySelectorAll('.stab').forEach(function (b) {
    b.addEventListener('click', function () {
      document.querySelectorAll('.stab').forEach(function (x) { x.classList.remove('active'); });
      b.classList.add('active');
      var f = b.dataset.sfilter;
      document.querySelectorAll('.product').forEach(function (card) {
        card.style.display = (f === 'all' || card.dataset.cat === f) ? '' : 'none';
      });
    });
  });

  /* contacts (config-driven, no dead buttons) */
  (function () {
    var box = document.getElementById('contactGrid');
    if (!box) return;
    var C = CFG.CONTACTS || {};
    var items = [
      { key: 'discord', name: 'Discord', sub: 'поддержка и анонсы обновлений', icon: '<path d="M7 17c-2 0-3.5-1.5-3.5-1.5C4.5 9 8 5.5 12 5.5s7.5 3.5 8.5 10c0 0-1.5 1.5-3.5 1.5l-1-2h-8z"/>' },
      { key: 'telegram', name: 'Telegram', sub: 'новости и розыгрыши ключей', icon: '<path d="M21 4L3 11l6 2 2 7 4-5 6 1z"/>' },
      { key: 'youtube', name: 'YouTube', sub: 'обзоры и гайды по настройке', icon: '<rect x="3" y="6" width="18" height="12" rx="4"/><path d="M10.5 9.5l5 2.5-5 2.5z" fill="currentColor"/>' }
    ];
    var html = '';
    items.forEach(function (it) {
      if (C[it.key]) {
        html += '<a class="contact-card" href="' + C[it.key] + '" target="_blank" rel="noopener">' +
          '<span class="contact-ico"><svg viewBox="0 0 24 24" fill="none" stroke-width="1.8">' + it.icon + '</svg></span>' +
          '<span><b>' + it.name + '</b><span>' + it.sub + '</span></span></a>';
      }
    });
    box.innerHTML = html ||
      '<div class="contact-soon">Каналы откроются перед релизом.<br>Прямо сейчас на связи — общий IRC внутри кабинета.<br><a class="btn" href="cabinet.html">Открыть кабинет</a></div>';
  })();

  /* ---- auth modal ---- */
  var authMode = 'reg';
  var modal = document.getElementById('authModal');
  var pendingBuy = null;
  function openAuth(mode) {
    authMode = mode || 'reg';
    var isReg = authMode === 'reg';
    document.getElementById('authTitle').textContent = isReg ? 'Регистрация' : 'Вход';
    document.getElementById('authSub').textContent = isReg
      ? 'UID выдаётся по порядку регистрации'
      : 'UID и подписка уже привязаны к твоему нику';
    document.getElementById('authSubmit').textContent = isReg ? 'Зарегистрироваться' : 'Войти';
    document.getElementById('authSwitch').textContent = isReg ? 'Уже есть аккаунт? Войти' : 'Нет аккаунта? Регистрация';
    document.getElementById('authEmailWrap').style.display = isReg ? '' : 'none';
    document.getElementById('authRefWrap').style.display = isReg ? '' : 'none';
    document.getElementById('authForgot').style.display = isReg ? 'none' : '';
    document.getElementById('authErr').textContent = '';
    modal.classList.add('show');
  }
  document.getElementById('authClose').addEventListener('click', function () { modal.classList.remove('show'); });
  modal.addEventListener('click', function (e) { if (e.target === modal) modal.classList.remove('show'); });
  document.getElementById('authSwitch').addEventListener('click', function () {
    openAuth(authMode === 'reg' ? 'login' : 'reg');
  });
  document.querySelectorAll('[data-auth]').forEach(function (b) {
    b.addEventListener('click', function () { openAuth(b.dataset.auth); });
  });
  document.getElementById('authSubmit').addEventListener('click', function () {
    var name = document.getElementById('authName').value;
    var pass = document.getElementById('authPass').value;
    var remember = document.getElementById('authRemember').checked;
    var p = authMode === 'reg'
      ? S.register(name, document.getElementById('authEmail').value, pass, remember, document.getElementById('authRef').value)
      : S.login(name, pass, remember);
    p.then(function (r) {
      if (r.error) { document.getElementById('authErr').textContent = r.error; return; }
      modal.classList.remove('show');
      toast('UID ' + r.user.uid + ' — добро пожаловать, ' + r.user.name);
      setTimeout(function () { location.href = 'cabinet.html'; }, 600);
    });
  });

  /* ---- recover modal ---- */
  var recModal = document.getElementById('recoverModal');
  document.getElementById('authForgot').addEventListener('click', function () {
    modal.classList.remove('show');
    document.getElementById('recErr').textContent = '';
    recModal.classList.add('show');
  });
  document.getElementById('recoverClose').addEventListener('click', function () { recModal.classList.remove('show'); });
  recModal.addEventListener('click', function (e) { if (e.target === recModal) recModal.classList.remove('show'); });
  document.getElementById('recSubmit').addEventListener('click', function () {
    S.recover(
      document.getElementById('recNick').value,
      document.getElementById('recEmail').value,
      document.getElementById('recPass').value
    ).then(function (r) {
      if (r.error) { document.getElementById('recErr').textContent = r.error; return; }
      recModal.classList.remove('show');
      toast('Пароль изменён. Войди с новым паролем.');
      openAuth('login');
    });
  });

  /* ---- card input: formatting + validation (shared PayCard) ---- */
  PayCard.wireNum(document.getElementById('cardNum'));
  PayCard.wireExp(document.getElementById('cardExp'));
  PayCard.wireCvc(document.getElementById('cardCvc'));
  function cardValid() {
    var r = PayCard.validate(
      document.getElementById('cardNum'),
      document.getElementById('cardExp'),
      document.getElementById('cardCvc'));
    if (!r.ok) return r.err;
    return '';
  }
  function cardHash() {
    return PayCard.validate(
      document.getElementById('cardNum'),
      document.getElementById('cardExp'),
      document.getElementById('cardCvc')).hash;
  }

  /* ---- trial removed ---- */

  /* ---- builds timeline ---- */
  S.builds().then(function (list) {
    var box = document.getElementById('buildsList');
    if (!box) return;
    box.innerHTML = list.map(function (b) {
      return '<div class="tl-item"><span class="tl-dot"></span>' +
        '<div class="tl-card"><div class="tl-top"><b>' + b.version + ' · ' + S.esc(b.title) + '</b><span>' + b.date + '</span></div>' +
        '<p>' + S.esc(b.text) + '</p></div></div>';
    }).join('');
  });

  /* ---- referral prefill (?ref=UID) ---- */
  var ref0 = '';
  try { ref0 = new URLSearchParams(location.search).get('ref') || ''; } catch (e) {}
  if (ref0) {
    var ri = document.getElementById('authRef');
    if (ri) ri.value = ref0.replace(/\D/g, '').slice(0, 8);
  }
  /* ---- bypass status ---- */
  (function () {
    var grid = document.getElementById('bypassGrid');
    if (!grid || !CFG.BYPASSES) return;
    var label = { work: 'работает', test: 'на тесте', fix: 'обновляем' };
    grid.innerHTML = CFG.BYPASSES.map(function (b) {
      return '<div class="bypass"><div class="bypass-top"><span class="dot ' + b.state + '"></span><b>' + b.server + '</b></div>' +
        '<small>' + b.mode + '</small>' +
        '<div class="ver"><span>' + b.version + ' · ' + b.updated + '</span><em>' + (label[b.state] || b.state) + '</em></div></div>';
    }).join('');
  })();

  S.me().then(function (me) {
    var hb = document.getElementById('headAuth');
    if (!hb) return;
    if (me) {
      hb.textContent = me.name + ' #' + me.uid;
      hb.addEventListener('click', function () { location.href = 'cabinet.html'; });
    } else {
      hb.addEventListener('click', function () { openAuth('login'); });
    }
  });

  /* ---- buy modal ---- */
  var buyPlan = null;
  var buyModal = document.getElementById('buyModal');
  document.querySelectorAll('[data-buy]').forEach(function (b) {
    b.addEventListener('click', function () {
      if (!S.hasSession()) {
        pendingBuy = b.dataset.buy;
        openAuth('reg');
        toast('Сначала зарегистрируйся — ключ привяжется к твоему UID');
        return;
      }
      buyPlan = b.dataset.buy;
      document.getElementById('buyPlan').textContent =
        (S.PLAN_LABEL[buyPlan] || buyPlan) + ' — ' + S.PRICES[buyPlan] + ' ₽. Ключ выдаётся сразу после оплаты.';
      document.getElementById('buyErr').textContent = '';
      S.billing().then(function (bl) {
        document.getElementById('buyBal').textContent = (bl.balance || 0) + ' ₽';
      });
      buyModal.classList.add('show');
    });
  });
  document.querySelectorAll('.paymethod').forEach(function (p) {
    p.addEventListener('click', function () {
      document.querySelectorAll('.paymethod').forEach(function (x) { x.classList.remove('active'); });
      p.classList.add('active');
      var bal = p.dataset.pay === 'balance';
      document.getElementById('cardFields').style.display = bal ? 'none' : '';
      document.getElementById('balHint').classList.toggle('hidden', !bal);
      document.getElementById('buyErr').textContent = '';
    });
  });
  document.getElementById('buyClose').addEventListener('click', function () { buyModal.classList.remove('show'); });
  buyModal.addEventListener('click', function (e) { if (e.target === buyModal) buyModal.classList.remove('show'); });
  document.querySelectorAll('.pay').forEach(function (p) {
    p.addEventListener('click', function () {
      document.querySelectorAll('.pay').forEach(function (x) { x.classList.remove('active'); });
      p.classList.add('active');
    });
  });
  document.getElementById('buyConfirm').addEventListener('click', function () {
    var payMode = document.querySelector('.paymethod.active');
    var pay = payMode ? payMode.dataset.pay : 'card';
    var hash = '';
    if (pay === 'card') {
      var err = cardValid();
      if (err) { document.getElementById('buyErr').textContent = err; return; }
      hash = cardHash();
    }
    document.getElementById('buyErr').textContent = '';
    S.buy(buyPlan, hash, pay).then(function (r) {
      if (r.error) { document.getElementById('buyErr').textContent = r.error; return; }
      buyModal.classList.remove('show');
      try { navigator.clipboard.writeText(r.code); } catch (e) {}
      toast('Оплачено' + (pay === 'balance' ? ' с баланса' : '') + '. Ключ: ' + r.code + ' (скопирован)');
      setTimeout(function () { location.href = 'cabinet.html'; }, 1200);
    });
  });

  document.addEventListener('keydown', function (e) {
    if (e.key === 'Escape') document.querySelectorAll('.modal').forEach(function (m) { m.classList.remove('show'); });
  });
})();
