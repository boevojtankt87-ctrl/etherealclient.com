/* Ethereal — market page */
(function () {
  'use strict';
  var S = window.EthStore;
  var curTag = '', curTab = 'all', lastList = [];

  /* bg grid */
  (function () {
    var c = document.getElementById('bg');
    if (!c) return;
    var x = c.getContext('2d'), W, H;
    function rs() { W = c.width = innerWidth; H = c.height = innerHeight; }
    rs(); addEventListener('resize', rs);
    x.clearRect(0, 0, W, H);
    var acc = getComputedStyle(document.documentElement).getPropertyValue('--acc').trim() || '#4eba8e';
    x.strokeStyle = acc + '12';
    var step = 72;
    x.beginPath();
    for (var gx = 0; gx < W + step; gx += step) { x.moveTo(gx, 0); x.lineTo(gx, H); }
    for (var gy = 0; gy < H + step; gy += step) { x.moveTo(0, gy); x.lineTo(W, gy); }
    x.stroke();
  })();

  /* header auth + balance */
  function paintHead() {
    S.me().then(function (me) {
      var hb = document.getElementById('headAuth');
      var lbl = document.getElementById('headAuthLabel');
      if (me) {
        if (lbl) lbl.textContent = me.name + ' #' + me.uid;
        else hb.textContent = me.name + ' #' + me.uid;
        hb.onclick = function () { location.href = 'cabinet.html'; };
        S.billing().then(function (b) {
          document.getElementById('mBal').textContent = (b.balance || 0) + ' ₽';
        });
      } else {
        hb.onclick = function () { location.href = 'cabinet.html'; };
      }
    });
  }
  paintHead();

  var burger = document.getElementById('burger');

  /* tabs */
  document.querySelectorAll('[data-mtab]').forEach(function (t) {
    t.addEventListener('click', function () {
      document.querySelectorAll('[data-mtab]').forEach(function (x) { x.classList.remove('active'); });
      t.classList.add('active');
      curTab = t.dataset.mtab;
      render();
    });
  });

  /* tags */
  S.marketList('', '').then(function (r) {
    var box = document.getElementById('mkTags');
    var tags = r.tags && r.tags.length ? r.tags : ['HolyWorld', 'FunTime', 'ReallyWorld', 'DonutSMP', 'Universal'];
    box.innerHTML = '<button class="mtag active" data-tag="">Все</button>' + tags.map(function (t) {
      return '<button class="mtag" data-tag="' + t + '">' + t + '</button>';
    }).join('');
    box.querySelectorAll('.mtag').forEach(function (b) {
      b.addEventListener('click', function () {
        box.querySelectorAll('.mtag').forEach(function (x) { x.classList.remove('active'); });
        b.classList.add('active');
        curTag = b.dataset.tag;
        render();
      });
    });
  });

  var searchTimer = null;
  document.getElementById('mkSearch').addEventListener('input', function () {
    clearTimeout(searchTimer);
    searchTimer = setTimeout(render, 350);
  });

  function lotCard(l) {
    return '<div class="lot"><span class="lot-tag">' + S.esc(l.tag) + ' · ' + l.date + '</span>' +
      '<h3>' + S.esc(l.title) + '</h3>' +
      '<div class="seller">продавец <b>' + S.esc(l.seller) + ' #' + l.sellerUid + '</b></div>' +
      '<div class="lot-foot"><span class="lot-price">' + l.price + ' ₽</span>' +
      '<span class="lot-sales">покупок: ' + l.sales + '</span></div>' +
      '<div class="lot-actions"><button class="btn btn-solid" data-buylot="' + l.id + '">Купить</button></div></div>';
  }

  function render() {
    var grid = document.getElementById('mkGrid');
    if (curTab === 'all') {
      S.marketList(curTag, document.getElementById('mkSearch').value).then(function (r) {
        lastList = r.listings || [];
        if (!lastList.length) {
          grid.innerHTML = '<div class="empty">Лотов пока нет. Стань первым — выставь свой конфиг.<br><button class="btn" id="emptyCreate">Выставить лот</button></div>';
          var ec = document.getElementById('emptyCreate');
          if (ec) ec.onclick = openCreate;
          return;
        }
        grid.innerHTML = lastList.map(lotCard).join('');
        wireBuy(grid);
      });
    } else {
      if (!S.hasSession()) {
        grid.innerHTML = '<div class="empty">Войди, чтобы увидеть свои лоты и покупки.<br><a class="btn" href="cabinet.html">Открыть кабинет</a></div>';
        return;
      }
      S.marketMy().then(function (r) {
        if (r.error) { grid.innerHTML = '<div class="empty">' + S.esc(r.error) + '</div>'; return; }
        if (curTab === 'mine') {
          if (!r.mine.length) {
            grid.innerHTML = '<div class="empty">У тебя нет лотов.<br><button class="btn" id="emptyCreate">Выставить лот</button></div>';
            var ec = document.getElementById('emptyCreate');
            if (ec) ec.onclick = openCreate;
            return;
          }
          grid.innerHTML = r.mine.map(function (l) {
            return '<div class="lot' + (l.active ? '' : ' off') + '"><span class="lot-tag">' + S.esc(l.tag) + ' · ' + l.date + '</span>' +
              '<h3>' + S.esc(l.title) + '</h3>' +
              '<div class="lot-foot"><span class="lot-price">' + l.price + ' ₽</span>' +
              '<span class="lot-sales">покупок: ' + l.sales + (l.active ? '' : ' · скрыт') + '</span></div>' +
              '<div class="lot-actions"><button class="btn" data-view="' + l.id + '">Конфиг</button>' +
              '<button class="btn" data-toggle="' + l.id + '" data-on="' + (l.active ? 0 : 1) + '">' + (l.active ? 'Скрыть' : 'Вернуть') + '</button></div></div>';
          }).join('');
          wireMine(grid);
        } else {
          if (!r.bought.length) {
            grid.innerHTML = '<div class="empty">Ты ещё ничего не покупал.</div>';
            return;
          }
          grid.innerHTML = r.bought.map(function (l) {
            return '<div class="lot"><span class="lot-tag">' + S.esc(l.tag) + ' · ' + l.date + '</span>' +
              '<h3>' + S.esc(l.title) + '</h3>' +
              '<div class="seller">продавец <b>' + S.esc(l.seller) + '</b> · куплен за ' + l.price + ' ₽</div>' +
              '<div class="lot-actions"><button class="btn btn-solid" data-view="' + l.id + '">Открыть конфиг</button></div></div>';
          }).join('');
          wireMine(grid);
        }
      });
    }
  }

  function wireBuy(grid) {
    grid.querySelectorAll('[data-buylot]').forEach(function (b) {
      b.addEventListener('click', function () {
        if (!S.hasSession()) { location.href = 'cabinet.html'; return; }
        b.disabled = true;
        S.marketBuy(+b.dataset.buylot).then(function (r) {
          b.disabled = false;
          if (r.error) {
            toast(r.error, true);
            if (r.error.indexOf('балансе') !== -1) setTimeout(function () { location.href = 'cabinet.html#balance'; }, 1400);
            return;
          }
          toast('Куплено. Конфиг открыт ниже.');
          paintHead();
          render();
          openView(r.title, '', '', r.config);
        });
      });
    });
  }

  function wireMine(grid) {
    grid.querySelectorAll('[data-view]').forEach(function (b) {
      b.addEventListener('click', function () {
        S.marketConfig(+b.dataset.view).then(function (r) {
          if (r.error) { toast(r.error, true); return; }
          openView(r.title, r.tag, '', r.config);
        });
      });
    });
    grid.querySelectorAll('[data-toggle]').forEach(function (b) {
      b.addEventListener('click', function () {
        S.marketToggle(+b.dataset.toggle, +b.dataset.on === 1).then(function (r) {
          if (r.error) { toast(r.error, true); return; }
          render();
        });
      });
    });
  }

  var cfgText = '';
  function openView(title, tag, descr, config) {
    document.getElementById('viewTitle').textContent = title;
    document.getElementById('viewMeta').textContent = tag;
    document.getElementById('viewDescr').textContent = descr || '';
    document.getElementById('viewDescr').style.display = descr ? '' : 'none';
    document.getElementById('viewConfig').textContent = config;
    cfgText = config;
    document.getElementById('viewModal').classList.add('show');
  }
  document.getElementById('viewClose').addEventListener('click', function () {
    document.getElementById('viewModal').classList.remove('show');
  });
  document.getElementById('viewModal').addEventListener('click', function (e) {
    if (e.target.id === 'viewModal') e.target.classList.remove('show');
  });
  document.getElementById('viewCopy').addEventListener('click', function () {
    try { navigator.clipboard.writeText(cfgText); toast('Конфиг скопирован'); }
    catch (e) { toast('Не вышло скопировать', true); }
  });

  /* create */
  var mkModal = document.getElementById('mkModal');
  function openCreate() {
    if (!S.hasSession()) { location.href = 'cabinet.html'; return; }
    document.getElementById('mkErr').textContent = '';
    mkModal.classList.add('show');
  }
  document.getElementById('mkCreate').addEventListener('click', openCreate);
  document.getElementById('mkClose').addEventListener('click', function () { mkModal.classList.remove('show'); });
  mkModal.addEventListener('click', function (e) { if (e.target === mkModal) mkModal.classList.remove('show'); });
  document.getElementById('mkPrice').addEventListener('input', function (e) {
    e.target.value = e.target.value.replace(/\D/g, '').slice(0, 6);
  });
  document.getElementById('mkSubmit').addEventListener('click', function () {
    S.marketCreate({
      title: document.getElementById('mkTitle').value,
      tag: document.getElementById('mkTag').value,
      price: document.getElementById('mkPrice').value,
      descr: document.getElementById('mkDescr').value,
      config: document.getElementById('mkConfig').value
    }).then(function (r) {
      if (r.error) { document.getElementById('mkErr').textContent = r.error; return; }
      mkModal.classList.remove('show');
      ['mkTitle', 'mkPrice', 'mkDescr', 'mkConfig'].forEach(function (id) { document.getElementById(id).value = ''; });
      toast('Лот #' + r.id + ' выставлен');
      document.querySelector('[data-mtab="mine"]').click();
    });
  });

  document.addEventListener('keydown', function (e) {
    if (e.key === 'Escape') document.querySelectorAll('.modal').forEach(function (m) { m.classList.remove('show'); });
  });

  render();
})();
