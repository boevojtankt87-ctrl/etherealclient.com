"""Etherial — SQLite layer. No third-party dependencies."""
import hashlib
import json
import os
import re
import secrets
import sqlite3
import time

PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'etherial.db')
ROLES = ['user', 'tester', 'youtube', 'tiktok', 'moderator', 'admin']
PLANS = {'TRIAL': 1, 'D7': 7, 'D30': 30, 'D90': 90, 'D180': 180}
BUYABLE = {'D7', 'D30', 'D90', 'D180'}
PRICES = {'D7': 100, 'D30': 300, 'D90': 550, 'D180': 800}
MARKET_FEE = 10  # percent kept by the project on market sales
MARKET_TAGS = ['HolyWorld', 'FunTime', 'ReallyWorld', 'DonutSMP', 'Universal']
DAY = 86400
EMAIL_RE = re.compile(r'^[^@\s]+@[^@\s]+\.[^@\s]+$')


def connect():
    con = sqlite3.connect(PATH)
    con.row_factory = sqlite3.Row
    return con


def init():
    con = connect()
    cur = con.cursor()
    cur.execute('''CREATE TABLE IF NOT EXISTS users(
        id INTEGER PRIMARY KEY, uid INTEGER UNIQUE, nick TEXT UNIQUE,
        pass TEXT, role TEXT, hwid TEXT, created INTEGER,
        sub_until INTEGER DEFAULT 0, plan TEXT)''')
    cur.execute('''CREATE TABLE IF NOT EXISTS sessions(
        token TEXT PRIMARY KEY, user_id INTEGER, created INTEGER)''')
    cur.execute('''CREATE TABLE IF NOT EXISTS keys(
        code TEXT PRIMARY KEY, plan TEXT, created INTEGER, used_by INTEGER)''')
    cur.execute('''CREATE TABLE IF NOT EXISTS chat(
        id INTEGER PRIMARY KEY AUTOINCREMENT, nick TEXT, uid INTEGER,
        role TEXT, ts INTEGER, text TEXT)''')
    cur.execute('''CREATE TABLE IF NOT EXISTS logs(
        id INTEGER PRIMARY KEY AUTOINCREMENT, user_id INTEGER, ts INTEGER, text TEXT)''')
    cur.execute('CREATE TABLE IF NOT EXISTS meta(k TEXT PRIMARY KEY, v TEXT)')
    cur.execute("INSERT OR IGNORE INTO meta(k,v) VALUES('uid_counter','1')")
    # migrations for older databases
    cols = [r['name'] for r in cur.execute('PRAGMA table_info(users)').fetchall()]
    if 'email' not in cols:
        cur.execute("ALTER TABLE users ADD COLUMN email TEXT DEFAULT ''")
    if 'trial_used' not in cols:
        cur.execute('ALTER TABLE users ADD COLUMN trial_used INTEGER DEFAULT 0')
    scol = [r['name'] for r in cur.execute('PRAGMA table_info(sessions)').fetchall()]
    if 'last_seen' not in scol:
        cur.execute('ALTER TABLE sessions ADD COLUMN last_seen INTEGER DEFAULT 0')
    ucols = [r['name'] for r in cur.execute('PRAGMA table_info(users)').fetchall()]
    if 'balance' not in ucols:
        cur.execute('ALTER TABLE users ADD COLUMN balance INTEGER DEFAULT 0')
    cur.execute('''CREATE TABLE IF NOT EXISTS cards(
        card_hash TEXT PRIMARY KEY, user_id INTEGER, created INTEGER)''')
    cur.execute('''CREATE TABLE IF NOT EXISTS builds(
        id INTEGER PRIMARY KEY AUTOINCREMENT, version TEXT, title TEXT,
        text TEXT, ts INTEGER)''')
    cur.execute('''CREATE TABLE IF NOT EXISTS polls(
        id INTEGER PRIMARY KEY AUTOINCREMENT, question TEXT, options TEXT,
        active INTEGER DEFAULT 1, ts INTEGER)''')
    cur.execute('''CREATE TABLE IF NOT EXISTS votes(
        poll_id INTEGER, user_id INTEGER, option_idx INTEGER,
        UNIQUE(poll_id, user_id))''')
    cur.execute('''CREATE TABLE IF NOT EXISTS transactions(
        id INTEGER PRIMARY KEY AUTOINCREMENT, user_id INTEGER, ts INTEGER,
        kind TEXT, amount INTEGER, text TEXT)''')
    cur.execute('''CREATE TABLE IF NOT EXISTS listings(
        id INTEGER PRIMARY KEY AUTOINCREMENT, seller_id INTEGER, title TEXT,
        server_tag TEXT, price INTEGER, descr TEXT, config TEXT,
        created INTEGER, active INTEGER DEFAULT 1, sales INTEGER DEFAULT 0)''')
    cur.execute('''CREATE TABLE IF NOT EXISTS purchases(
        buyer_id INTEGER, listing_id INTEGER, price INTEGER, ts INTEGER,
        UNIQUE(buyer_id, listing_id))''')
    cur.execute('''CREATE TABLE IF NOT EXISTS irc_reqs(
        id INTEGER PRIMARY KEY AUTOINCREMENT, from_id INTEGER, to_id INTEGER,
        created INTEGER, UNIQUE(from_id, to_id))''')
    cur.execute('''CREATE TABLE IF NOT EXISTS irc_friends(
        u1 INTEGER, u2 INTEGER, created INTEGER, UNIQUE(u1, u2))''')
    cur.execute('''CREATE TABLE IF NOT EXISTS irc_dms(
        id INTEGER PRIMARY KEY AUTOINCREMENT, from_id INTEGER, to_id INTEGER,
        text TEXT, ts INTEGER)''')
    # admin is only ever zxs
    cur.execute("UPDATE users SET role='user' WHERE role='admin' AND LOWER(nick)!='zxs'")
    # initial changelog — factual project history, admin extends it later
    if not cur.execute('SELECT 1 FROM builds LIMIT 1').fetchone():
        now = int(time.time())
        cur.execute('INSERT INTO builds(version,title,text,ts) VALUES(?,?,?,?)',
                    ('v1.0', 'Релиз Etherial',
                     'Первый публичный билд: ClickGUI, Aura/Speed/ESP-пресеты, облачные конфиги, IRC.',
                     now - 40 * 86400))
        cur.execute('INSERT INTO builds(version,title,text,ts) VALUES(?,?,?,?)',
                    ('v1.4.2', 'Обходы HolyWorld и FunTime',
                     'Переписаны ротации Aura и глайды ElytraFly. Пресеты лежат в облаке.',
                     now - 3 * 86400))
    if not cur.execute('SELECT 1 FROM polls WHERE active=1 LIMIT 1').fetchone():
        cur.execute('INSERT INTO polls(question,options,active,ts) VALUES(?,?,1,?)',
                    ('Какой обход обновить следующим?',
                     json.dumps(['DonutSMP Fly', 'ReallyWorld Speed', 'FunTime Aura', 'Новый TargetESP']),
                     now - 2 * 86400))
    con.commit()
    con.close()


# ---------- passwords ----------

def hash_pass(password):
    salt = secrets.token_hex(16)
    h = hashlib.pbkdf2_hmac('sha256', password.encode(), bytes.fromhex(salt), 200_000).hex()
    return salt + '$' + h


def check_pass(password, stored):
    try:
        salt, h = stored.split('$')
    except ValueError:
        return False
    calc = hashlib.pbkdf2_hmac('sha256', password.encode(), bytes.fromhex(salt), 200_000).hex()
    return secrets.compare_digest(calc, h)


def gen_hwid():
    abc = 'ABCDEFGHJKLMNPQRSTUVWXYZ23456789'
    return '-'.join(''.join(secrets.choice(abc) for _ in range(4)) for _ in range(3))


def gen_key_code():
    abc = 'ABCDEFGHJKLMNPQRSTUVWXYZ23456789'
    part = lambda: ''.join(secrets.choice(abc) for _ in range(4))
    return 'ETHR-%s-%s' % (part(), part())


# ---------- users ----------

def public_user(row):
    return {'name': row['nick'], 'uid': row['uid'], 'role': row['role'],
            'email': row['email'] if 'email' in row.keys() else '',
            'balance': row['balance'] if 'balance' in row.keys() and row['balance'] else 0,
            'hwid': row['hwid'], 'created': row['created'] * 1000,
            'subUntil': (row['sub_until'] or 0) * 1000, 'plan': row['plan']}


def get_user_by_token(token):
    if not token:
        return None
    con = connect()
    row = con.execute('''SELECT u.* FROM users u
        JOIN sessions s ON s.user_id=u.id WHERE s.token=?''', (token,)).fetchone()
    if row:
        con.execute('UPDATE sessions SET last_seen=? WHERE token=?', (int(time.time()), token))
        con.commit()
    con.close()
    return row


def online_count():
    con = connect()
    n = con.execute('SELECT COUNT(DISTINCT user_id) c FROM sessions WHERE last_seen>?',
                    (int(time.time()) - 900,)).fetchone()['c']
    con.close()
    return n


def register(nick, email, password):
    con = connect()
    cur = con.cursor()
    if cur.execute('SELECT 1 FROM users WHERE LOWER(nick)=LOWER(?)', (nick,)).fetchone():
        con.close()
        return None, 'Этот ник уже занят'
    if not EMAIL_RE.match(email or ''):
        con.close()
        return None, 'Введи корректную почту — она нужна для восстановления'
    if cur.execute('SELECT 1 FROM users WHERE LOWER(email)=LOWER(?)', (email,)).fetchone():
        con.close()
        return None, 'Эта почта уже привязана к другому аккаунту'
    counter = int(cur.execute("SELECT v FROM meta WHERE k='uid_counter'").fetchone()['v'])
    role = 'admin' if nick.lower() == 'zxs' else 'user'
    cur.execute('''INSERT INTO users(uid,nick,email,pass,role,hwid,created,sub_until,plan,trial_used)
        VALUES(?,?,?,?,?,NULL,?,0,NULL,0)''',
                (counter, nick, email, hash_pass(password), role, int(time.time())))
    user_id = cur.lastrowid
    cur.execute("UPDATE meta SET v=? WHERE k='uid_counter'", (str(counter + 1),))
    token = secrets.token_hex(24)
    cur.execute('INSERT INTO sessions(token,user_id,created) VALUES(?,?,?)',
                (token, user_id, int(time.time())))
    add_log(cur, user_id, 'аккаунт создан, UID %d' % counter)
    con.commit()
    row = cur.execute('SELECT * FROM users WHERE id=?', (user_id,)).fetchone()
    con.close()
    return {'token': token, 'user': public_user(row)}, None


def login(nick, password):
    con = connect()
    cur = con.cursor()
    row = cur.execute('SELECT * FROM users WHERE LOWER(nick)=LOWER(?)', (nick,)).fetchone()
    if not row or not check_pass(password, row['pass']):
        con.close()
        return None, 'Неверный ник или пароль'
    token = secrets.token_hex(24)
    cur.execute('INSERT INTO sessions(token,user_id,created) VALUES(?,?,?)',
                (token, row['id'], int(time.time())))
    add_log(cur, row['id'], 'вход в кабинет')
    con.commit()
    con.close()
    return {'token': token, 'user': public_user(row)}, None


def logout(token):
    con = connect()
    con.execute('DELETE FROM sessions WHERE token=?', (token,))
    con.commit()
    con.close()


def add_log(cur, user_id, text):
    cur.execute('INSERT INTO logs(user_id,ts,text) VALUES(?,?,?)',
                (user_id, int(time.time()), text))


def user_logs(user_id):
    con = connect()
    rows = con.execute('SELECT ts,text FROM logs WHERE user_id=? ORDER BY id DESC LIMIT 40',
                       (user_id,)).fetchall()
    con.close()
    import datetime
    out = []
    for r in rows:
        dt = datetime.datetime.fromtimestamp(r['ts']).strftime('%d.%m.%Y, %H:%M:%S')
        out.append('<b>%s</b> — %s' % (dt, r['text']))
    return out


# ---------- keys / subscription ----------

def mint_key(plan):
    con = connect()
    code = gen_key_code()
    con.execute('INSERT INTO keys(code,plan,created,used_by) VALUES(?,?,?,NULL)',
                (code, plan, int(time.time())))
    con.commit()
    con.close()
    return code


def activate(user_row, raw):
    import re
    code = (raw or '').strip().upper()
    if not re.match(r'^ETHR-[A-Z0-9]{4}-[A-Z0-9]{4}$', code):
        return None, 'Неверный формат. Пример: ETHR-AB12-CD34'
    con = connect()
    cur = con.cursor()
    k = cur.execute('SELECT * FROM keys WHERE code=?', (code,)).fetchone()
    if not k:
        con.close()
        return None, 'Ключ не найден. Проверь код или купи тариф.'
    if k['used_by']:
        con.close()
        return None, 'Этот ключ уже использован'
    days = PLANS.get(k['plan'], 30)
    base = max(int(time.time()), user_row['sub_until'] or 0)
    cur.execute('UPDATE users SET sub_until=?, plan=? WHERE id=?',
                (base + days * DAY, k['plan'], user_row['id']))
    cur.execute('UPDATE keys SET used_by=? WHERE code=?', (user_row['uid'], code))
    add_log(cur, user_row['id'], 'ключ %s активирован → %s (+%d дн.)' % (code, k['plan'], days))
    con.commit()
    con.close()
    return {'plan': k['plan'], 'days': days}, None


def reset_hwid(user_row):
    con = connect()
    cur = con.cursor()
    cur.execute('UPDATE users SET hwid=NULL WHERE id=?', (user_row['id'],))
    add_log(cur, user_row['id'], 'привязка HWID сброшена (оплата 150₽), привяжется при следующем запуске')
    con.commit()
    con.close()
    return 'bound-on-next-launch'


def client_launch(user_row, hwid):
    """HWID binds on first client launch under own login.
    Later launches from other hardware are rejected until HWID reset."""
    hwid = (hwid or '').strip().upper()
    if not re.match(r'^[A-Z0-9]{4}-[A-Z0-9]{4}-[A-Z0-9]{4}$', hwid):
        return None, 'Лаунчер передал пустой HWID'
    con = connect()
    cur = con.cursor()
    u = cur.execute('SELECT * FROM users WHERE id=?', (user_row['id'],)).fetchone()
    if not u['hwid']:
        cur.execute('UPDATE users SET hwid=? WHERE id=?', (hwid, u['id']))
        add_log(cur, u['id'], 'железо привязано при запуске клиента: ' + hwid)
        con.commit()
        con.close()
        return {'bound': True, 'hwid': hwid}, None
    if u['hwid'] != hwid:
        con.close()
        return None, 'Другое железо. Сбрось HWID в кабинете (150₽) и запускай снова.'
    add_log(cur, u['id'], 'запуск клиента подтверждён')
    con.commit()
    con.close()
    return {'bound': False, 'hwid': hwid}, None


def recover(nick, email, newpass):
    if not newpass or len(newpass) < 4:
        return 'Новый пароль: минимум 4 символа'
    con = connect()
    cur = con.cursor()
    u = cur.execute('SELECT * FROM users WHERE LOWER(nick)=LOWER(?)', (nick,)).fetchone()
    if not u or not u['email'] or u['email'].lower() != (email or '').strip().lower():
        con.close()
        return 'Пара ник + почта не найдена. Проверь данные.'
    cur.execute('UPDATE users SET pass=? WHERE id=?', (hash_pass(newpass), u['id']))
    cur.execute('DELETE FROM sessions WHERE user_id=?', (u['id'],))
    add_log(cur, u['id'], 'пароль восстановлен через почту')
    con.commit()
    con.close()
    return None


# ---------- billing: balance, topup, market ----------

def add_tx(cur, user_id, kind, amount, text):
    cur.execute('INSERT INTO transactions(user_id,ts,kind,amount,text) VALUES(?,?,?,?,?)',
                (user_id, int(time.time()), kind, amount, text))


def billing(user_row):
    con = connect()
    bal = con.execute('SELECT balance FROM users WHERE id=?', (user_row['id'],)).fetchone()['balance'] or 0
    rows = con.execute('SELECT ts,kind,amount,text FROM transactions WHERE user_id=? ORDER BY id DESC LIMIT 30',
                       (user_row['id'],)).fetchall()
    con.close()
    import datetime
    txs = [{'date': datetime.datetime.fromtimestamp(r['ts']).strftime('%d.%m.%y %H:%M'),
            'kind': r['kind'], 'amount': r['amount'], 'text': r['text']} for r in rows]
    return {'balance': bal, 'txs': txs}


def topup(user_row, amount, card_hash):
    try:
        amount = int(amount)
    except (TypeError, ValueError):
        return None, 'Сумма от 50 до 100000 ₽'
    if not 50 <= amount <= 100000:
        return None, 'Сумма от 50 до 100000 ₽'
    card_err = bind_card(card_hash or '', user_row)
    if card_err:
        return None, card_err
    con = connect()
    cur = con.cursor()
    cur.execute('UPDATE users SET balance=COALESCE(balance,0)+? WHERE id=?', (amount, user_row['id']))
    add_tx(cur, user_row['id'], 'topup', amount, 'пополнение баланса')
    add_log(cur, user_row['id'], 'баланс пополнен на %d ₽' % amount)
    con.commit()
    bal = cur.execute('SELECT balance FROM users WHERE id=?', (user_row['id'],)).fetchone()['balance']
    con.close()
    return {'balance': bal}, None


def buy_plan(user_row, plan, pay, card_hash):
    if plan not in BUYABLE:
        return None, 'Нет такого тарифа'
    price = PRICES[plan]
    con = connect()
    cur = con.cursor()
    if pay == 'balance':
        bal = cur.execute('SELECT balance FROM users WHERE id=?', (user_row['id'],)).fetchone()['balance'] or 0
        if bal < price:
            con.close()
            return None, 'Не хватает %d ₽ на балансе. Пополни в кабинете.' % (price - bal)
        cur.execute('UPDATE users SET balance=balance-? WHERE id=?', (price, user_row['id']))
        add_tx(cur, user_row['id'], 'sub_buy', -price, 'подписка %s' % plan)
    else:
        card_err = bind_card(card_hash or '', user_row)
        if card_err:
            con.close()
            return None, card_err
    code = gen_key_code()
    cur.execute('INSERT INTO keys(code,plan,created,used_by) VALUES(?,?,?,NULL)',
                (code, plan, int(time.time())))
    if pay == 'balance':
        add_log(cur, user_row['id'], 'оплата %s с баланса, выдан ключ %s' % (plan, code))
    else:
        add_log(cur, user_row['id'], 'оплата %s, выдан ключ %s (демо)' % (plan, code))
    con.commit()
    con.close()
    return {'code': code, 'plan': plan}, None


def market_create(user_row, title, tag, price, descr, config):
    title = (title or '').strip()
    descr = (descr or '').strip()
    config = (config or '').strip()
    if not 4 <= len(title) <= 80:
        return None, 'Название: 4–80 символов'
    if tag not in MARKET_TAGS:
        return None, 'Выбери сервер из списка'
    try:
        price = int(price)
    except (TypeError, ValueError):
        return None, 'Цена: 10–100000 ₽'
    if not 10 <= price <= 100000:
        return None, 'Цена: 10–100000 ₽'
    if not 10 <= len(descr) <= 2000:
        return None, 'Описание: 10–2000 символов'
    if not 10 <= len(config) <= 20000:
        return None, 'Вставь текст конфига (10–20000 символов)'
    con = connect()
    cur = con.cursor()
    cur.execute('''INSERT INTO listings(seller_id,title,server_tag,price,descr,config,created,active,sales)
        VALUES(?,?,?,?,?,?,?,1,0)''',
                (user_row['id'], title, tag, price, descr, config, int(time.time())))
    lid = cur.lastrowid
    add_log(cur, user_row['id'], 'лот #%d выставлен на маркет (%d ₽)' % (lid, price))
    con.commit()
    con.close()
    return {'id': lid}, None


def market_list(tag=None, q=None):
    con = connect()
    sql = '''SELECT l.id,l.title,l.server_tag,l.price,l.sales,l.created,u.nick,u.uid
        FROM listings l JOIN users u ON u.id=l.seller_id WHERE l.active=1'''
    args = []
    if tag in MARKET_TAGS:
        sql += ' AND l.server_tag=?'
        args.append(tag)
    if q:
        sql += ' AND l.title LIKE ?'
        args.append('%' + q.strip()[:40] + '%')
    sql += ' ORDER BY l.id DESC LIMIT 60'
    rows = con.execute(sql, args).fetchall()
    con.close()
    import datetime
    return [{'id': r['id'], 'title': r['title'], 'tag': r['server_tag'], 'price': r['price'],
             'sales': r['sales'], 'seller': r['nick'], 'sellerUid': r['uid'],
             'date': datetime.datetime.fromtimestamp(r['created']).strftime('%d.%m.%y')} for r in rows]


def market_buy(buyer_row, listing_id):
    try:
        listing_id = int(listing_id)
    except (TypeError, ValueError):
        return None, 'Нет такого лота'
    con = connect()
    cur = con.cursor()
    listing = cur.execute('SELECT * FROM listings WHERE id=?', (listing_id,)).fetchone()
    if not listing or not listing['active']:
        con.close()
        return None, 'Лот снят с продажи'
    if listing['seller_id'] == buyer_row['id']:
        con.close()
        return None, 'Это твой собственный лот'
    if cur.execute('SELECT 1 FROM purchases WHERE buyer_id=? AND listing_id=?',
                   (buyer_row['id'], listing_id)).fetchone():
        con.close()
        return None, 'Ты уже купил этот конфиг — он во вкладке «Купленные»'
    bal = cur.execute('SELECT balance FROM users WHERE id=?', (buyer_row['id'],)).fetchone()['balance'] or 0
    price = listing['price']
    if bal < price:
        con.close()
        return None, 'Не хватает %d ₽ на балансе. Пополни в кабинете.' % (price - bal)
    seller_cut = price * (100 - MARKET_FEE) // 100
    cur.execute('UPDATE users SET balance=balance-? WHERE id=?', (price, buyer_row['id']))
    cur.execute('UPDATE users SET balance=COALESCE(balance,0)+? WHERE id=?', (seller_cut, listing['seller_id']))
    cur.execute('INSERT INTO purchases(buyer_id,listing_id,price,ts) VALUES(?,?,?,?)',
                (buyer_row['id'], listing_id, price, int(time.time())))
    cur.execute('UPDATE listings SET sales=sales+1 WHERE id=?', (listing_id,))
    seller = cur.execute('SELECT nick FROM users WHERE id=?', (listing['seller_id'],)).fetchone()
    add_tx(cur, buyer_row['id'], 'market_buy', -price, 'конфиг «%s»' % listing['title'][:40])
    add_tx(cur, listing['seller_id'], 'market_sale', seller_cut,
           'продажа «%s» (комиссия %d%%)' % (listing['title'][:40], MARKET_FEE))
    add_log(cur, buyer_row['id'], 'куплен конфиг «%s» за %d ₽' % (listing['title'][:40], price))
    add_log(cur, listing['seller_id'], 'продан конфиг «%s» (+%d ₽)' % (listing['title'][:40], seller_cut))
    con.commit()
    con.close()
    return {'title': listing['title'], 'config': listing['config'],
            'seller': seller['nick'] if seller else '?'}, None


def market_my(user_row):
    con = connect()
    mine = con.execute('''SELECT id,title,server_tag,price,sales,active,created FROM listings
        WHERE seller_id=? ORDER BY id DESC''', (user_row['id'],)).fetchall()
    bought = con.execute('''SELECT l.id,l.title,l.server_tag,p.price,p.ts,u.nick FROM purchases p
        JOIN listings l ON l.id=p.listing_id JOIN users u ON u.id=l.seller_id
        WHERE p.buyer_id=? ORDER BY p.ts DESC''', (user_row['id'],)).fetchall()
    con.close()
    import datetime
    return {
        'mine': [{'id': r['id'], 'title': r['title'], 'tag': r['server_tag'], 'price': r['price'],
                  'sales': r['sales'], 'active': bool(r['active']),
                  'date': datetime.datetime.fromtimestamp(r['created']).strftime('%d.%m.%y')} for r in mine],
        'bought': [{'id': r['id'], 'title': r['title'], 'tag': r['server_tag'], 'price': r['price'],
                    'seller': r['nick'],
                    'date': datetime.datetime.fromtimestamp(r['ts']).strftime('%d.%m.%y')} for r in bought],
    }


def market_config(user_row, listing_id):
    try:
        listing_id = int(listing_id)
    except (TypeError, ValueError):
        return None, 'Нет такого лота'
    con = connect()
    row = con.execute('''SELECT l.title,l.config,l.server_tag FROM listings l
        LEFT JOIN purchases p ON p.listing_id=l.id AND p.buyer_id=?
        WHERE l.id=? AND (p.buyer_id IS NOT NULL OR l.seller_id=?)''',
                      (user_row['id'], listing_id, user_row['id'])).fetchone()
    con.close()
    if not row:
        return None, 'Сначала купи этот конфиг'
    return {'title': row['title'], 'tag': row['server_tag'], 'config': row['config']}, None


def market_toggle(user_row, listing_id, active):
    try:
        listing_id = int(listing_id)
    except (TypeError, ValueError):
        return None, 'Нет такого лота'
    con = connect()
    cur = con.cursor()
    listing = cur.execute('SELECT * FROM listings WHERE id=? AND seller_id=?',
                          (listing_id, user_row['id'])).fetchone()
    if not listing:
        con.close()
        return None, 'Не твой лот'
    cur.execute('UPDATE listings SET active=? WHERE id=?', (1 if active else 0, listing_id))
    add_log(cur, user_row['id'], 'лот #%d %s' % (listing_id, 'возвращён на витрину' if active else 'снят с продажи'))
    con.commit()
    con.close()
    return {'ok': True}, None


# ---------- builds (changelog) ----------

def get_builds():
    con = connect()
    rows = con.execute('SELECT version,title,text,ts FROM builds ORDER BY id DESC LIMIT 20').fetchall()
    con.close()
    import datetime
    return [{'version': r['version'], 'title': r['title'], 'text': r['text'],
             'date': datetime.datetime.fromtimestamp(r['ts']).strftime('%d.%m.%y')} for r in rows]


def add_build(version, title, text):
    version = (version or '').strip()[:16]
    title = (title or '').strip()[:120]
    text = (text or '').strip()[:2000]
    if not version or not title or not text:
        return 'Заполни версию, заголовок и текст'
    con = connect()
    con.execute('INSERT INTO builds(version,title,text,ts) VALUES(?,?,?,?)',
                (version, title, text, int(time.time())))
    con.commit()
    con.close()
    return None


# ---------- polls ----------

def get_poll(token):
    con = connect()
    p = con.execute('SELECT * FROM polls WHERE active=1 ORDER BY id DESC LIMIT 1').fetchone()
    if not p:
        con.close()
        return None
    options = json.loads(p['options'])
    counts = [0] * len(options)
    for r in con.execute('SELECT option_idx, COUNT(*) c FROM votes WHERE poll_id=? GROUP BY option_idx',
                         (p['id'],)).fetchall():
        if 0 <= r['option_idx'] < len(counts):
            counts[r['option_idx']] = r['c']
    my = None
    if token:
        u = con.execute('''SELECT u.id FROM users u JOIN sessions s ON s.user_id=u.id
            WHERE s.token=?''', (token,)).fetchone()
        if u:
            v = con.execute('SELECT option_idx FROM votes WHERE poll_id=? AND user_id=?',
                            (p['id'], u['id'])).fetchone()
            if v:
                my = v['option_idx']
    con.close()
    return {'id': p['id'], 'question': p['question'], 'options': options,
            'counts': counts, 'total': sum(counts), 'my': my}


def vote(token, option_idx):
    con = connect()
    u = con.execute('''SELECT u.* FROM users u JOIN sessions s ON s.user_id=u.id
        WHERE s.token=?''', (token,)).fetchone() if token else None
    if not u:
        con.close()
        return 'Войди, чтобы голосовать'
    p = con.execute('SELECT * FROM polls WHERE active=1 ORDER BY id DESC LIMIT 1').fetchone()
    if not p:
        con.close()
        return 'Нет активного опроса'
    try:
        idx = int(option_idx)
    except (TypeError, ValueError):
        idx = -1
    if not 0 <= idx < len(json.loads(p['options'])):
        con.close()
        return 'Нет такого варианта'
    try:
        con.execute('INSERT INTO votes(poll_id,user_id,option_idx) VALUES(?,?,?)',
                    (p['id'], u['id'], idx))
        con.commit()
    except sqlite3.IntegrityError:
        con.close()
        return 'Ты уже голосовал в этом опросе'
    con.close()
    return None


def add_poll(question, options_csv):
    question = (question or '').strip()[:160]
    options = [o.strip()[:60] for o in (options_csv or '').split(',') if o.strip()][:5]
    if not question or len(options) < 2:
        return 'Нужен вопрос и минимум 2 варианта через запятую'
    con = connect()
    con.execute('UPDATE polls SET active=0 WHERE active=1')
    con.execute('INSERT INTO polls(question,options,active,ts) VALUES(?,?,1,?)',
                (question, json.dumps(options), int(time.time())))
    con.commit()
    con.close()
    return None


# ---------- chat ----------

def get_chat():
    con = connect()
    rows = con.execute('SELECT id, nick, uid, role, ts, text FROM chat ORDER BY id DESC LIMIT 60').fetchall()
    con.close()
    rows = list(rows)[::-1]
    return [{'id': r['id'], 'n': r['nick'], 'uid': r['uid'], 'role': r['role'],
             't': r['ts'] * 1000, 'text': r['text']} for r in rows]


def post_chat(user_row, text):
    text = (text or '').strip()[:500]
    if not text:
        return False
    con = connect()
    cur = con.execute('INSERT INTO chat(nick,uid,role,ts,text) VALUES(?,?,?,?,?)',
                      (user_row['nick'], user_row['uid'], user_row['role'], int(time.time()), text))
    con.commit()
    msg = {'id': cur.lastrowid, 'n': user_row['nick'], 'uid': user_row['uid'],
           'role': user_row['role'], 't': int(time.time()) * 1000, 'text': text}
    con.close()
    return msg


# ---------- irc ----------

def _nick_row(con, nick):
    return con.execute('SELECT * FROM users WHERE LOWER(nick)=LOWER(?)',
                       ((nick or '').strip(),)).fetchone()


def _irc_online(con, user_id):
    return con.execute('SELECT 1 FROM sessions WHERE user_id=? AND last_seen>?',
                       (user_id, int(time.time()) - 900)).fetchone() is not None


def _irc_user(r):
    return {'username': r['nick'], 'role': r['role'], 'uid': r['uid']}


def irc_global_msg(r):
    return {'id': r['id'], 'n': r['nick'], 'uid': r['uid'], 'role': r['role'],
            't': r['ts'] * 1000, 'text': r['text']}


def irc_dm_msg(r, me_id):
    con = connect()
    fn = con.execute('SELECT nick FROM users WHERE id=?', (r['from_id'],)).fetchone()
    tn = con.execute('SELECT nick FROM users WHERE id=?', (r['to_id'],)).fetchone()
    con.close()
    return {'id': r['id'], 'from': fn['nick'] if fn else '?', 'to': tn['nick'] if tn else '?',
            'text': r['text'], 't': r['ts'] * 1000, 'self': r['from_id'] == me_id}


def irc_friend_lists(user_row):
    con = connect()
    uid = user_row['id']
    friends = []
    for r in con.execute('''SELECT u.*, f.created c FROM users u JOIN irc_friends f
            ON (f.u1=? AND f.u2=u.id) OR (f.u2=? AND f.u1=u.id)
            ORDER BY LOWER(u.nick)''', (uid, uid)):
        d = _irc_user(r)
        d['isOnline'] = _irc_online(con, r['id'])
        last = con.execute('''SELECT text FROM irc_dms
                WHERE (from_id=? AND to_id=?) OR (from_id=? AND to_id=?)
                ORDER BY id DESC LIMIT 1''', (uid, r['id'], r['id'], uid)).fetchone()
        d['lastMessage'] = last['text'] if last else ''
        friends.append(d)
    incoming = []
    for r in con.execute('''SELECT u.*, q.created c FROM irc_reqs q JOIN users u ON u.id=q.from_id
            WHERE q.to_id=? ORDER BY q.id DESC''', (uid,)):
        d = _irc_user(r)
        d['timestamp'] = r['c'] * 1000
        incoming.append(d)
    outgoing = []
    for r in con.execute('''SELECT u.*, q.created c FROM irc_reqs q JOIN users u ON u.id=q.to_id
            WHERE q.from_id=? ORDER BY q.id DESC''', (uid,)):
        d = _irc_user(r)
        d['timestamp'] = r['c'] * 1000
        outgoing.append(d)
    con.close()
    return friends, incoming, outgoing


def irc_friend_add(user_row, nick):
    con = connect()
    target = _nick_row(con, nick)
    if not target:
        con.close()
        return 'Игрок не найден'
    if target['id'] == user_row['id']:
        con.close()
        return 'Нельзя добавить себя'
    a, b = sorted((user_row['id'], target['id']))
    if con.execute('SELECT 1 FROM irc_friends WHERE u1=? AND u2=?', (a, b)).fetchone():
        con.close()
        return 'Уже в друзьях'
    if con.execute('''SELECT 1 FROM irc_reqs WHERE (from_id=? AND to_id=?)
            OR (from_id=? AND to_id=?)''', (user_row['id'], target['id'],
                                            target['id'], user_row['id'])).fetchone():
        con.close()
        return 'Заявка уже отправлена'
    con.execute('INSERT OR IGNORE INTO irc_reqs(from_id,to_id,created) VALUES(?,?,?)',
                (user_row['id'], target['id'], int(time.time())))
    con.commit()
    con.close()
    return None


def irc_friend_accept(user_row, nick):
    con = connect()
    target = _nick_row(con, nick)
    if not target:
        con.close()
        return 'Игрок не найден'
    req = con.execute('SELECT 1 FROM irc_reqs WHERE from_id=? AND to_id=?',
                      (target['id'], user_row['id'])).fetchone()
    if not req:
        con.close()
        return 'Нет заявки от этого игрока'
    a, b = sorted((user_row['id'], target['id']))
    con.execute('INSERT OR IGNORE INTO irc_friends(u1,u2,created) VALUES(?,?,?)',
                (a, b, int(time.time())))
    con.execute('DELETE FROM irc_reqs WHERE (from_id=? AND to_id=?) OR (from_id=? AND to_id=?)',
                (target['id'], user_row['id'], user_row['id'], target['id']))
    con.commit()
    con.close()
    return None


def irc_friend_decline(user_row, nick):
    con = connect()
    target = _nick_row(con, nick)
    if not target:
        con.close()
        return 'Игрок не найден'
    con.execute('DELETE FROM irc_reqs WHERE (from_id=? AND to_id=?) OR (from_id=? AND to_id=?)',
                (target['id'], user_row['id'], user_row['id'], target['id']))
    con.commit()
    con.close()
    return None


def irc_friend_remove(user_row, nick):
    con = connect()
    target = _nick_row(con, nick)
    if not target:
        con.close()
        return 'Игрок не найден'
    a, b = sorted((user_row['id'], target['id']))
    con.execute('DELETE FROM irc_friends WHERE u1=? AND u2=?', (a, b))
    con.commit()
    con.close()
    return None


def irc_dm_send(user_row, to_nick, text):
    text = (text or '').strip()[:500]
    if not text:
        return None, 'Пустое сообщение'
    con = connect()
    target = _nick_row(con, to_nick)
    if not target:
        con.close()
        return None, 'Игрок не найден'
    if target['id'] == user_row['id']:
        con.close()
        return None, 'Нельзя писать себе'
    now = int(time.time())
    cur = con.execute('INSERT INTO irc_dms(from_id,to_id,text,ts) VALUES(?,?,?,?)',
                      (user_row['id'], target['id'], text, now))
    con.commit()
    msg = {'id': cur.lastrowid, 'from': user_row['nick'], 'to': target['nick'],
           'text': text, 't': now * 1000, 'self': True}
    con.close()
    return msg, None


def irc_dm_history(user_row, other_nick, limit=100):
    con = connect()
    other = _nick_row(con, other_nick)
    if not other:
        con.close()
        return None
    rows = con.execute('''SELECT * FROM irc_dms WHERE (from_id=? AND to_id=?)
            OR (from_id=? AND to_id=?) ORDER BY id DESC LIMIT ?''',
                       (user_row['id'], other['id'], other['id'], user_row['id'], limit)).fetchall()
    con.close()
    rows = list(rows)[::-1]
    me = user_row['id']
    return [{'id': r['id'], 'from': user_row['nick'] if r['from_id'] == me else other['nick'],
             'to': other['nick'] if r['from_id'] == me else user_row['nick'],
             'text': r['text'], 't': r['ts'] * 1000, 'self': r['from_id'] == me} for r in rows]


def irc_poll(user_row, g_id, d_id):
    con = connect()
    me = user_row['id']
    g = [irc_global_msg(r) for r in con.execute(
        '''SELECT c.id, u.nick, u.uid, u.role, c.ts, c.text FROM chat c
           JOIN users u ON u.uid=c.uid
           WHERE c.id>? ORDER BY c.id LIMIT 300''', (g_id,))]
    dms = []
    for r in con.execute('''SELECT * FROM irc_dms WHERE id>? AND (from_id=? OR to_id=?)
            ORDER BY id LIMIT 300''', (d_id, me, me)):
        fn = con.execute('SELECT nick FROM users WHERE id=?', (r['from_id'],)).fetchone()
        tn = con.execute('SELECT nick FROM users WHERE id=?', (r['to_id'],)).fetchone()
        dms.append({'id': r['id'], 'from': fn['nick'] if fn else '?',
                    'to': tn['nick'] if tn else '?', 'text': r['text'],
                    't': r['ts'] * 1000, 'self': r['from_id'] == me})
    online = con.execute('SELECT COUNT(DISTINCT user_id) c FROM sessions WHERE last_seen>?',
                         (int(time.time()) - 900,)).fetchone()['c']
    con.close()
    friends, incoming, outgoing = irc_friend_lists(user_row)
    return {'global': g, 'dms': dms, 'friends': friends, 'incoming': incoming,
            'outgoing': outgoing, 'online': online}


# ---------- admin ----------

def admin_overview():
    con = connect()
    users = con.execute('SELECT * FROM users ORDER BY uid').fetchall()
    unused = con.execute('SELECT COUNT(*) c FROM keys WHERE used_by IS NULL').fetchone()['c']
    now = int(time.time())
    active = sum(1 for u in users if (u['sub_until'] or 0) > now)
    out = []
    for u in users:
        left = max(0, (u['sub_until'] or 0) - now)
        out.append({'key': u['nick'].lower(), 'name': u['nick'], 'uid': u['uid'],
                    'role': u['role'], 'plan': u['plan'], 'leftMs': left * 1000})
    con.close()
    return {'total': len(out), 'active': active, 'unusedKeys': unused, 'users': out}


def set_role(admin_row, nick, role):
    if role not in ROLES:
        return 'Неизвестная роль'
    con = connect()
    cur = con.cursor()
    u = cur.execute('SELECT * FROM users WHERE LOWER(nick)=LOWER(?)', (nick,)).fetchone()
    if not u:
        con.close()
        return 'Пользователь не найден'
    if u['id'] == admin_row['id'] and role != 'admin':
        con.close()
        return 'Нельзя снять роль с себя'
    if role == 'admin' and u['nick'].lower() != 'zxs':
        con.close()
        return 'Админ может быть только zxs'
    cur.execute('UPDATE users SET role=? WHERE id=?', (role, u['id']))
    add_log(cur, admin_row['id'], 'роль %s → %s' % (u['nick'], role))
    con.commit()
    con.close()
    return None


def delete_user(admin_row, nick):
    con = connect()
    cur = con.cursor()
    u = cur.execute('SELECT * FROM users WHERE LOWER(nick)=LOWER(?)', (nick,)).fetchone()
    if not u:
        con.close()
        return 'Пользователь не найден'
    if u['id'] == admin_row['id']:
        con.close()
        return 'Нельзя удалить себя'
    cur.execute('DELETE FROM sessions WHERE user_id=?', (u['id'],))
    cur.execute('DELETE FROM logs WHERE user_id=?', (u['id'],))
    cur.execute('DELETE FROM users WHERE id=?', (u['id'],))
    add_log(cur, admin_row['id'], 'удалён пользователь %s' % u['nick'])
    con.commit()
    con.close()
    return None


def stats():
    con = connect()
    total = con.execute('SELECT COUNT(*) c FROM users').fetchone()['c']
    con.close()
    return {'users': total, 'online': online_count()}


def bind_card(card_hash, user_row):
    """One card = one account. Returns error string or None."""
    if not card_hash or len(card_hash) != 64:
        return 'Введи данные карты'
    try:
        int(card_hash, 16)
    except ValueError:
        return 'Введи данные карты'
    con = connect()
    cur = con.cursor()
    row = cur.execute('SELECT * FROM cards WHERE card_hash=?', (card_hash,)).fetchone()
    if row and row['user_id'] != user_row['id']:
        other = cur.execute('SELECT uid FROM users WHERE id=?', (row['user_id'],)).fetchone()
        con.close()
        uid = other['uid'] if other else '?'
        return 'Эта карта уже привязана к UID #%s. Одна карта — один аккаунт.' % uid
    if not row:
        cur.execute('INSERT INTO cards(card_hash,user_id,created) VALUES(?,?,?)',
                    (card_hash, user_row['id'], int(time.time())))
        add_log(cur, user_row['id'], 'привязана карта •• %s' % card_hash[:6])
        con.commit()
    con.close()
    return None
