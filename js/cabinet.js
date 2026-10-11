/* Etherial — cabinet page: profile / IRC / settings / admin (API-backed) */
(function () {
  'use strict';
  var S = window.EthStore;
  var me = null;

  function needLogin() {
    document.getElementById('guestBox').classList.remove('hidden');
    document.getElementById('cabLayout').classList.add('hidden');
  }
  function showCab() {
    document.getElementById('guestBox').classList.add('hidden');
    document.getElementById('cabLayout').classList.remove('hidden');
  }

  /* ---------- guest auth ---------- */
  var gMode = 'reg';
  function paintGuest() {
    document.querySelectorAll('.auth-tabs button').forEach(function (b) {
      b.classList.toggle('active', b.dataset.gmode === gMode);
    });
    document.getElementById('gSubmit').textContent = gMode === 'reg' ? 'Зарегистрироваться' : 'Войти';
    document.getElementById('gEmailWrap').style.display = gMode === 'reg' ? '' : 'none';
    document.getElementById('gErr').textContent = '';
  }
  document.querySelectorAll('.auth-tabs button').forEach(function (b) {
    b.addEventListener('click', function () { gMode = b.dataset.gmode; paintGuest(); });
  });
  paintGuest();

  function doGuestAuth() {
    var remember = document.getElementById('gRemember').checked;
    var p = gMode === 'reg'
      ? S.register(document.getElementById('gName').value, document.getElementById('gEmail').value, document.getElementById('gPass').value, remember)
      : S.login(document.getElementById('gName').value, document.getElementById('gPass').value, remember);
    p.then(function (r) {
      if (r.error) { document.getElementById('gErr').textContent = r.error; return; }
      location.reload();
    });
  }
  document.getElementById('gSubmit').addEventListener('click', doGuestAuth);
  document.getElementById('gPass').addEventListener('keydown', function (e) { if (e.key === 'Enter') doGuestAuth(); });

  /* ---------- tabs ---------- */
  document.querySelectorAll('.cab-tab').forEach(function (t) {
    t.addEventListener('click', function () {
      document.querySelectorAll('.cab-tab').forEach(function (x) { x.classList.remove('active'); });
      t.classList.add('active');
      var tab = t.dataset.tab;
      document.querySelectorAll('.cab-panel').forEach(function (p) {
        p.classList.toggle('hidden', p.id !== 'panel-' + tab);
      });
      if (tab === 'irc') renderChat(true);
      if (tab === 'balance') renderBalance();
      if (tab === 'admin') renderAdmin();
    });
  });

  document.addEventListener('keydown', function (e) {
    if (e.key === 'Escape') document.querySelectorAll('.modal').forEach(function (m) { m.classList.remove('show'); });
  });

  S.me().then(function (u) {
    if (!u) { needLogin(); return; }
    me = u;
    showCab();
    initCabinet();
  });

  function initCabinet() {
    document.getElementById('sideAva').textContent = me.name[0].toUpperCase();
    document.getElementById('sideName').textContent = me.name;
    document.getElementById('sideUid').textContent = '#' + me.uid + ' · ' + me.role;
    if (me.role !== 'admin') {
      var at = document.querySelector('[data-tab="admin"]');
      if (at) at.classList.add('hidden');
    }
    renderProfile();

    document.getElementById('copyUid').addEventListener('click', function () {
      try { navigator.clipboard.writeText('#' + me.uid); toast('UID #' + me.uid + ' скопирован'); }
      catch (e) { toast('#' + me.uid); }
    });

    document.getElementById('activateBtn').addEventListener('click', function () {
      S.activate(document.getElementById('keyInput').value).then(function (r) {
        if (r.error) { toast(r.error, true); return; }
        document.getElementById('keyInput').value = '';
        refresh().then(function () { toast('Тариф ' + S.PLAN_LABEL[r.plan] + ' активирован (+' + r.days + ' дн.)'); });
      });
    });

    /* HWID reset — paid */
    var hwidModal = document.getElementById('hwidModal');
    document.getElementById('hwidBtn').addEventListener('click', function () { hwidModal.classList.add('show'); });
    document.getElementById('hwidClose').addEventListener('click', function () { hwidModal.classList.remove('show'); });
    hwidModal.addEventListener('click', function (e) { if (e.target === hwidModal) hwidModal.classList.remove('show'); });
    document.getElementById('hwidConfirm').addEventListener('click', function () {
      S.hwidReset().then(function (r) {
        if (r.error) { toast(r.error, true); return; }
        hwidModal.classList.remove('show');
        refresh().then(function () { toast('Привязка сброшена. HWID привяжется при следующем запуске клиента.'); });
      });
    });

  /* loader download: real client jar from the site files */
  document.getElementById('dlBtn').addEventListener('click', function () {
    if (S.subLeft(me) <= 0) { toast('Сначала активируй ключ — клиент выдаётся с подпиской', true); return; }
    document.getElementById('dlUid').textContent = '#' + me.uid;
    document.getElementById('dlModal').classList.add('show');
  });
  document.getElementById('dlClose').addEventListener('click', function () {
    document.getElementById('dlModal').classList.remove('show');
  });
  document.getElementById('dlModal').addEventListener('click', function (e) {
    if (e.target.id === 'dlModal') e.target.classList.remove('show');
  });

    document.getElementById('logoutBtn').addEventListener('click', function () {
      S.logout().then(function () { location.reload(); });
    });

    /* referral card */
    (function () {
      var base = location.href.split('?')[0];
      document.getElementById('refLink').value = base + '?ref=' + me.uid;
      S.refStats().then(function (r) {
        if (r.error) return;
        document.getElementById('refStats').textContent =
          'Приглашено: ' + r.invited + ' · заработано бонусом: ' + r.earnedDays + ' дн. (+' + 3 + ' дн. за каждого с ключом)';
      });
    })();
    document.getElementById('refCopy').addEventListener('click', function () {
      try { navigator.clipboard.writeText(document.getElementById('refLink').value); toast('Реферальная ссылка скопирована'); }
      catch (e) { toast(document.getElementById('refLink').value); }
    });

    /* IRC */
    renderChat(true);
    document.getElementById('chatSend').addEventListener('click', sendChat);
    document.getElementById('chatInput').addEventListener('keydown', function (e) { if (e.key === 'Enter') sendChat(); });
    setInterval(function () {
      if (!document.getElementById('panel-irc').classList.contains('hidden')) renderChat(false);
    }, 5000);

    /* settings */
    paintSettings();
    document.querySelectorAll('.theme-card').forEach(function (c) {
      c.addEventListener('click', function () {
        localStorage.setItem('eth_theme', c.dataset.theme);
        window.EthTheme.apply(); paintSettings();
        toast('Тема применена');
      });
    });
    document.querySelectorAll('.acc-dot').forEach(function (d) {
      d.addEventListener('click', function () {
        localStorage.setItem('eth_accent', d.dataset.color);
        window.EthTheme.apply(); paintSettings();
      });
    });
    document.getElementById('accCustom').addEventListener('input', function (e) {
      localStorage.setItem('eth_accent', e.target.value);
      window.EthTheme.apply(); paintSettings();
    });
    document.getElementById('motionSw').addEventListener('click', function () {
      var off = localStorage.getItem('eth_motion') !== 'off';
      localStorage.setItem('eth_motion', off ? 'off' : 'on');
      window.EthTheme.apply(); paintSettings();
    });
    document.getElementById('themeReset').addEventListener('click', function () {
      localStorage.removeItem('eth_theme');
      localStorage.removeItem('eth_accent');
      localStorage.removeItem('eth_motion');
      window.EthTheme.apply(); paintSettings();
      toast('Оформление сброшено');
    });

  var genBtn = document.getElementById('genKeyBtn');
  if (genBtn) genBtn.addEventListener('click', function () {
    S.adminKey(document.getElementById('genPlan').value).then(function (r) {
      if (r.error) { toast(r.error, true); return; }
      document.getElementById('genOut').textContent = r.code;
      try { navigator.clipboard.writeText(r.code); } catch (e) {}
      renderAdmin();
    });
  });
  var buildAdd = document.getElementById('buildAdd');
  if (buildAdd) buildAdd.addEventListener('click', function () {
    S.adminBuild(
      document.getElementById('buildVer').value,
      document.getElementById('buildTitle').value,
      document.getElementById('buildText').value
    ).then(function (r) {
      if (r.error) { toast(r.error, true); return; }
      document.getElementById('buildVer').value = '';
      document.getElementById('buildTitle').value = '';
      document.getElementById('buildText').value = '';
      toast('Запись опубликована в журнале');
    });
  });

  function refresh() {
    return S.me().then(function (u) { me = u; renderProfile(); });
  }

  /* ---------- profile ---------- */
  function renderProfile() {
    document.getElementById('pAva').textContent = me.name[0].toUpperCase();
    document.getElementById('pName').textContent = me.name;
    var rb = document.getElementById('roleBadge');
    rb.textContent = me.role;
    rb.className = 'role-badge ' + me.role;
    document.getElementById('pUid').textContent = '#' + me.uid;
    document.getElementById('pEmail').textContent = me.email || '—';
    document.getElementById('pHwid').textContent = me.hwid || 'не привязан';
    document.getElementById('pDate').textContent = new Date(me.created).toLocaleDateString('ru-RU');
    document.getElementById('pDays').textContent =
      Math.floor((Date.now() - me.created) / 864e5) + ' дн. с нами';
    var left = S.subLeft(me);
    var badge = document.getElementById('pSubBadge');
    if (left > 0) {
      badge.textContent = (S.PLAN_LABEL[me.plan] || me.plan) + ' · осталось ' + S.fmtLeft(left);
      document.getElementById('pSub').textContent = S.PLAN_LABEL[me.plan] || me.plan;
      document.getElementById('pSubExp').textContent = 'до ' + new Date(me.subUntil).toLocaleString('ru-RU');
    } else {
      badge.textContent = 'без подписки';
      document.getElementById('pSub').textContent = '—';
      document.getElementById('pSubExp').textContent = 'активируй ключ ниже';
    }
    document.getElementById('cabLog').innerHTML =
      (me.log && me.log.length ? me.log.slice().reverse().map(function (l) { return '<div>' + l + '</div>'; }).join('') : '<div>пусто</div>');
    document.getElementById('sideUid').textContent = '#' + me.uid + ' · ' + me.role;
  }

  /* ---------- IRC ---------- */
  function msgHtml(m) {
    var t = new Date(m.t).toLocaleString('ru-RU', { day: '2-digit', month: '2-digit', hour: '2-digit', minute: '2-digit' });
    return '<div class="msg"><div class="msg-head"><b>' + S.esc(m.n) + '</b>' +
      '<span class="role-badge ' + S.esc(m.role || 'user') + '">' + S.esc(m.role || 'user') + '</span>' +
      '<span class="uid">#' + m.uid + '</span><time>' + t + '</time></div><p>' + S.esc(m.text) + '</p></div>';
  }
  function renderChat(scroll) {
    S.chat().then(function (list) {
      var box = document.getElementById('chatBox');
      var nearBottom = box.scrollTop + box.clientHeight >= box.scrollHeight - 60;
      box.innerHTML = list.length
        ? list.map(msgHtml).join('')
        : '<div class="msg"><p style="color:var(--dim)">Пока тихо. Напиши первым — сообщение увидят все владельцы.</p></div>';
      if (scroll || nearBottom) box.scrollTop = box.scrollHeight;
    });
  }
  function sendChat() {
    var inp = document.getElementById('chatInput');
    S.post(inp.value).then(function (r) {
      if (r.error) { toast(r.error, true); return; }
      inp.value = '';
      renderChat(true);
    });
  }

  /* ---------- balance ---------- */
  function renderBalance() {
    S.billing().then(function (b) {
      if (b.error) { toast(b.error, true); return; }
      document.getElementById('balAmount').textContent = (b.balance || 0) + ' ₽';
      document.getElementById('txLog').innerHTML = (b.txs && b.txs.length)
        ? b.txs.map(function (t) {
            var cls = t.amount >= 0 ? 'plus' : 'minus';
            var sign = t.amount >= 0 ? '+' : '';
            return '<div class="tx-row"><time>' + S.esc(t.date) + '</time><span>' + S.esc(t.text) + '</span><b class="' + cls + '">' + sign + t.amount + ' ₽</b></div>';
          }).join('')
        : '<div>пусто — пополни баланс выше</div>';
    });
  }
  PayCard.wireNum(document.getElementById('topCard'));
  PayCard.wireExp(document.getElementById('topExp'));
  PayCard.wireCvc(document.getElementById('topCvc'));
  document.getElementById('topAmount').addEventListener('input', function (e) {
    e.target.value = e.target.value.replace(/\D/g, '').slice(0, 6);
  });
  document.querySelectorAll('#topAmounts .amt').forEach(function (b) {
    b.addEventListener('click', function () {
      document.querySelectorAll('#topAmounts .amt').forEach(function (x) { x.classList.remove('sel'); });
      b.classList.add('sel');
      document.getElementById('topAmount').value = b.dataset.amt;
    });
  });
  document.getElementById('topSubmit').addEventListener('click', function () {
    var v = PayCard.validate(
      document.getElementById('topCard'),
      document.getElementById('topExp'),
      document.getElementById('topCvc'));
    if (!v.ok) { document.getElementById('topErr').textContent = v.err; return; }
    var amt = parseInt(document.getElementById('topAmount').value, 10);
    if (!amt || amt < 50 || amt > 100000) {
      document.getElementById('topErr').textContent = 'Сумма от 50 до 100000 ₽';
      return;
    }
    document.getElementById('topErr').textContent = '';
    S.topup(amt, v.hash).then(function (r) {
      if (r.error) { document.getElementById('topErr').textContent = r.error; return; }
      toast('Баланс пополнен: +' + amt + ' ₽');
      renderBalance();
    });
  });

  /* hash routing: cabinet.html#balance */
  if ((location.hash || '') === '#balance') {
    var bt = document.querySelector('[data-tab="balance"]');
    if (bt) bt.click();
  }

  /* ---------- settings ---------- */
  function paintSettings() {
    var theme = localStorage.getItem('eth_theme') || 'etherial';
    document.querySelectorAll('.theme-card').forEach(function (c) {
      c.classList.toggle('active', c.dataset.theme === theme);
    });
    var defAcc = { etherial: '#4eba8e', mono: '#ededed', crimson: '#e0334f', toxic: '#b8ff29', royal: '#a678ff', arctic: '#0284c7' }[theme] || '#4eba8e';
    var acc = (localStorage.getItem('eth_accent') || defAcc).toLowerCase();
    document.querySelectorAll('.acc-dot').forEach(function (d) {
      d.classList.toggle('active', d.dataset.color.toLowerCase() === acc);
    });
    try { document.getElementById('accCustom').value = acc; } catch (e) {}
    document.getElementById('motionSw').classList.toggle('on', localStorage.getItem('eth_motion') !== 'off');
  }

  /* ---------- admin ---------- */
  function renderAdmin() {
    if (!me || me.role !== 'admin') return;
    S.adminOverview().then(function (r) {
      if (r.error) { toast(r.error, true); return; }
      document.getElementById('stUsers').textContent = r.total;
      document.getElementById('stActive').textContent = r.active;
      document.getElementById('stKeys').textContent = r.unusedKeys;
      var tb = document.getElementById('usersBody');
      tb.innerHTML = '';
      r.users.forEach(function (u) {
        var tr = document.createElement('tr');
        var sel = '<select data-u="' + S.esc(u.key) + '">' + S.ROLES.map(function (role) {
          return '<option value="' + role + '"' + (u.role === role ? ' selected' : '') + '>' + role + '</option>';
        }).join('') + '</select>';
        var sub = u.leftMs > 0 ? (S.esc(u.plan || '') + ' · ' + S.fmtLeft(u.leftMs)) : '—';
        tr.innerHTML = '<td><b>' + S.esc(u.name) + '</b></td><td>#' + u.uid + '</td>' +
          '<td>' + sel + '</td><td>' + sub + '</td>' +
          '<td><button class="del" data-del="' + S.esc(u.key) + '">удалить</button></td>';
        tb.appendChild(tr);
      });
      tb.querySelectorAll('select').forEach(function (sel) {
        sel.addEventListener('change', function () {
          S.adminRole(sel.dataset.u, sel.value).then(function (res) {
            if (res.error) toast(res.error, true);
            else toast(sel.dataset.u + ' → ' + sel.value);
            renderAdmin();
          });
        });
      });
      tb.querySelectorAll('[data-del]').forEach(function (b) {
        b.addEventListener('click', function () {
          if (!confirm('Удалить пользователя ' + b.dataset.del + '?')) return;
          S.adminDelete(b.dataset.del).then(function (res) {
            if (res.error) toast(res.error, true);
            else toast('Пользователь удалён');
            renderAdmin();
          });
        });
      });
    });
  }
})();
