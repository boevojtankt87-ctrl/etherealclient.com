<?php
/* Ethereal API — PHP port of server.py for shared hosting (Timeweb).
   Endpoints and JSON shapes are identical to the Python server.
   Storage: SQLite in ./data by default; MySQL if config.php fills $MYSQL. */
declare(strict_types=1);

date_default_timezone_set('Europe/Moscow');

header('Content-Type: application/json; charset=utf-8');
header('Access-Control-Allow-Origin: *');
header('Access-Control-Allow-Headers: Content-Type, Authorization');
header('Access-Control-Allow-Methods: GET, POST, OPTIONS');
header('X-Content-Type-Options: nosniff');

if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') {
    http_response_code(204);
    exit;
}

const DAY = 86400;
const MARKET_FEE = 10;
const MARKET_TAGS = 'HolyWorld,FunTime,ReallyWorld,DonutSMP,Universal';
const PLANS = ['TRIAL' => 1, 'D7' => 7, 'D30' => 30, 'D90' => 90, 'D180' => 180];
const BUYABLE = ['D7', 'D30', 'D90', 'D180'];
const PRICES = ['D7' => 100, 'D30' => 300, 'D90' => 550, 'D180' => 800];
const ROLES = ['user', 'tester', 'youtube', 'tiktok', 'moderator', 'admin'];
const NICK_RE = '/^[A-Za-z0-9_]{3,16}$/';
const EMAIL_RE = '/^[^@\s]+@[^@\s]+\.[^@\s]+$/';

/* ---------- optional config (MySQL) ---------- */
$MYSQL = ['host' => '', 'name' => '', 'user' => '', 'pass' => ''];
if (is_file(__DIR__ . '/config.php')) {
    $cfg = [];
    include __DIR__ . '/config.php'; // sets $MYSQL
    if (!empty($cfg)) $MYSQL = array_merge($MYSQL, $cfg);
}

/* ---------- db layer ---------- */
function driver(string $set = '?'): string {
    static $d = 'sqlite';
    if ($set !== '?') $d = $set;
    return $d;
}
function db(): PDO {
    static $pdo = null;
    if ($pdo !== null) return $pdo;
    $opts = [PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION,
             PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_ASSOC];
    $m = $GLOBALS['MYSQL'] ?? ['host' => '', 'name' => '', 'user' => '', 'pass' => ''];
    if (!empty($m['host']) && extension_loaded('pdo_mysql')) {
        $pdo = new PDO("mysql:host={$m['host']};dbname={$m['name']};charset=utf8mb4",
                       $m['user'], $m['pass'], $opts);
        driver('mysql');
    } else {
        if (!extension_loaded('pdo_sqlite')) {
            out(['error' => 'На хостинге нет расширения pdo_sqlite и не задан MySQL (config.php).'], 500);
        }
        $dir = __DIR__ . '/data';
        if (!is_dir($dir) && !@mkdir($dir, 0775, true)) {
            $dir = sys_get_temp_dir(); // last resort: shared /tmp
        }
        if (!file_exists($dir . '/ethereal.db') && file_exists($dir . '/etherial.db')) {
            @rename($dir . '/etherial.db', $dir . '/ethereal.db'); // legacy brand rename
        }
        $pdo = new PDO('sqlite:' . $dir . '/ethereal.db', null, null, $opts);
        $pdo->exec('PRAGMA journal_mode=WAL');
        $pdo->exec('PRAGMA busy_timeout=8000');
        driver('sqlite');
    }
    init_schema($pdo);
    return $pdo;
}
function my(): bool { return driver() === 'mysql'; }

function init_schema(PDO $pdo): void {
    static $done = false;
    if ($done) return;
    $done = true;
    $ai = my() ? 'BIGINT NOT NULL AUTO_INCREMENT PRIMARY KEY' : 'INTEGER PRIMARY KEY AUTOINCREMENT';
    $pdo->exec("CREATE TABLE IF NOT EXISTS users(
        id $ai, uid INTEGER UNIQUE, nick TEXT UNIQUE, pass TEXT, role TEXT,
        hwid TEXT, created INTEGER, sub_until INTEGER DEFAULT 0, plan TEXT,
        email TEXT DEFAULT '', trial_used INTEGER DEFAULT 0,
        balance INTEGER DEFAULT 0, banned INTEGER DEFAULT 0)");
    $pdo->exec("CREATE TABLE IF NOT EXISTS sessions(
        token VARCHAR(64) PRIMARY KEY, user_id INTEGER, created INTEGER, last_seen INTEGER DEFAULT 0)");
    $pdo->exec("CREATE TABLE IF NOT EXISTS keys(
        code VARCHAR(24) PRIMARY KEY, plan TEXT, created INTEGER, used_by INTEGER)");
    $pdo->exec("CREATE TABLE IF NOT EXISTS chat(
        id $ai, nick TEXT, uid INTEGER, role TEXT, ts INTEGER, text TEXT)");
    $pdo->exec("CREATE TABLE IF NOT EXISTS logs(
        id $ai, user_id INTEGER, ts INTEGER, text TEXT)");
    $pdo->exec("CREATE TABLE IF NOT EXISTS meta(k VARCHAR(32) PRIMARY KEY, v TEXT)");
    $pdo->exec("CREATE TABLE IF NOT EXISTS cards(
        card_hash VARCHAR(64) PRIMARY KEY, user_id INTEGER, created INTEGER)");
    $pdo->exec("CREATE TABLE IF NOT EXISTS builds(
        id $ai, version TEXT, title TEXT, text TEXT, ts INTEGER)");
    $pdo->exec("CREATE TABLE IF NOT EXISTS transactions(
        id $ai, user_id INTEGER, ts INTEGER, kind TEXT, amount INTEGER, text TEXT)");
    $pdo->exec("CREATE TABLE IF NOT EXISTS listings(
        id $ai, seller_id INTEGER, title TEXT, server_tag TEXT, price INTEGER,
        descr TEXT, config TEXT, created INTEGER, active INTEGER DEFAULT 1, sales INTEGER DEFAULT 0)");
    $pdo->exec("CREATE TABLE IF NOT EXISTS purchases(
        buyer_id INTEGER, listing_id INTEGER, price INTEGER, ts INTEGER,
        UNIQUE(buyer_id, listing_id))");
    $pdo->exec("CREATE TABLE IF NOT EXISTS banned_hwids(
        hwid VARCHAR(32) PRIMARY KEY, by_nick TEXT, note TEXT DEFAULT '', created INTEGER)");
    try { $pdo->exec('ALTER TABLE users ADD COLUMN banned INTEGER DEFAULT 0'); }
    catch (Throwable $e) { /* column already exists */ }
    if (!$pdo->query("SELECT 1 FROM meta WHERE k='uid_counter'")->fetch()) {
        $pdo->prepare("INSERT INTO meta(k,v) VALUES('uid_counter','1')")->execute();
    }
    if (!$pdo->query('SELECT 1 FROM builds LIMIT 1')->fetch()) {
        $st = $pdo->prepare('INSERT INTO builds(version,title,text,ts) VALUES(?,?,?,?)');
        $now = time();
        $st->execute(['v1.0', 'Релиз Ethereal',
            'Первый публичный билд: ClickGUI, Aura/Speed/ESP-пресеты, облачные конфиги, IRC.',
            $now - 40 * DAY]);
        $st->execute(['v1.4.2', 'Обходы HolyWorld и FunTime',
            'Переписаны ротации Aura и глайды ElytraFly. Пресеты лежат в облаке.',
            $now - 3 * DAY]);
    }
}

/* ---------- helpers ---------- */
function out($arr, int $code = 200): void {
    http_response_code($code);
    echo json_encode($arr, JSON_UNESCAPED_UNICODE);
    exit;
}
function body(): array {
    $raw = file_get_contents('php://input');
    if ($raw === '' || strlen($raw) > 65536) return [];
    $j = json_decode($raw, true);
    return is_array($j) ? $j : [];
}
function reqToken(): string {
    $h = $_SERVER['HTTP_AUTHORIZATION'] ?? '';
    if (strncmp($h, 'Bearer ', 7) === 0) return trim(substr($h, 7));
    return '';
}
function hash_pass(string $password): string {
    $salt = bin2hex(random_bytes(16));
    $h = hash_pbkdf2('sha256', $password, hex2bin($salt), 200000, 0, true);
    return $salt . '$' . bin2hex($h);
}
function check_pass(string $password, string $stored): bool {
    $p = explode('$', $stored);
    if (count($p) !== 2) return false;
    $calc = hash_pbkdf2('sha256', $password, hex2bin($p[0]), 200000, 0, true);
    return hash_equals($p[1], bin2hex($calc));
}
function usub(string $s, int $max): string {
    if (function_exists('mb_substr')) return mb_substr($s, 0, $max);
    return preg_match('/^.{0,' . $max . '}/us', $s, $m) ? $m[0] : substr($s, 0, $max);
}
function gen_abc(int $n): string {
    $abc = 'ABCDEFGHJKLMNPQRSTUVWXYZ23456789';
    $s = '';
    for ($i = 0; $i < $n; $i++) $s .= $abc[random_int(0, strlen($abc) - 1)];
    return $s;
}
function gen_hwid(): string {
    return gen_abc(4) . '-' . gen_abc(4) . '-' . gen_abc(4);
}
function gen_key_code(): string {
    return 'ETHR-' . gen_abc(4) . '-' . gen_abc(4);
}
function public_user(array $u): array {
    return ['name' => $u['nick'], 'uid' => (int)$u['uid'], 'role' => $u['role'],
            'email' => $u['email'] ?? '', 'balance' => (int)($u['balance'] ?? 0),
            'hwid' => $u['hwid'], 'created' => (int)$u['created'] * 1000,
            'subUntil' => (int)($u['sub_until'] ?? 0) * 1000, 'plan' => $u['plan'],
            'banned' => (int)($u['banned'] ?? 0)];
}
function user_by_token(string $token): ?array {
    if ($token === '') return null;
    $pdo = db();
    $st = $pdo->prepare('SELECT u.* FROM users u JOIN sessions s ON s.user_id=u.id WHERE s.token=?');
    $st->execute([$token]);
    $u = $st->fetch();
    if ($u) {
        $pdo->prepare('UPDATE sessions SET last_seen=? WHERE token=?')->execute([time(), $token]);
    }
    return $u ?: null;
}
function me_or_die(): array {
    $u = user_by_token(reqToken());
    if (!$u) out(['error' => 'auth'], 401);
    if ((int)($u['banned'] ?? 0)) out(['error' => 'Аккаунт заблокирован администратором'], 403);
    return $u;
}
function admin_or_die(array $u): void {
    if (($u['role'] ?? '') !== 'admin') out(['error' => 'Только для админа'], 403);
}
function add_log(PDO $pdo, int $userId, string $text): void {
    $pdo->prepare('INSERT INTO logs(user_id,ts,text) VALUES(?,?,?)')->execute([$userId, time(), $text]);
}
function add_tx(PDO $pdo, int $userId, string $kind, int $amount, string $text): void {
    $pdo->prepare('INSERT INTO transactions(user_id,ts,kind,amount,text) VALUES(?,?,?,?,?)')
        ->execute([$userId, time(), $kind, $amount, $text]);
}
function md1(int|string $ts): string { return date('d.m.y', (int)$ts); }
function md2(int|string $ts): string { return date('d.m.Y, H:i:s', (int)$ts); }
function mdtx(int|string $ts): string { return date('d.m.y H:i', (int)$ts); }

/* one card = one account */
function bind_card(string $card_hash, array $u): ?string {
    if (strlen($card_hash) !== 64 || !ctype_xdigit($card_hash)) return 'Введи данные карты';
    $pdo = db();
    $st = $pdo->prepare('SELECT * FROM cards WHERE card_hash=?');
    $st->execute([$card_hash]);
    $row = $st->fetch();
    if ($row && (int)$row['user_id'] !== (int)$u['id']) {
        $q = $pdo->prepare('SELECT uid FROM users WHERE id=?');
        $q->execute([$row['user_id']]);
        $other = $q->fetch();
        return 'Эта карта уже привязана к UID #' . ($other ? $other['uid'] : '?') . '. Одна карта — один аккаунт.';
    }
    if (!$row) {
        $pdo->prepare('INSERT INTO cards(card_hash,user_id,created) VALUES(?,?,?)')
            ->execute([$card_hash, $u['id'], time()]);
        add_log($pdo, (int)$u['id'], 'привязана карта •• ' . substr($card_hash, 0, 6));
    }
    return null;
}

/* ---------- route ---------- */
$method = $_SERVER['REQUEST_METHOD'];
$path = parse_url($_SERVER['REQUEST_URI'], PHP_URL_PATH) ?: '/';
$path = rtrim($path, '/');
$data = $method === 'POST' ? body() : [];

/* ================= GET ================= */
if ($method === 'GET') {
    if ($path === '/api/me') {
        $u = user_by_token(reqToken());
        if (!$u) out(['error' => 'auth']);
        $out = public_user($u);
        $st = db()->prepare('SELECT ts,text FROM logs WHERE user_id=? ORDER BY id DESC LIMIT 40');
        $st->execute([$u['id']]);
        $log = [];
        foreach ($st->fetchAll() as $r) $log[] = '<b>' . md2($r['ts']) . '</b> — ' . $r['text'];
        $out['log'] = $log;
        out(['user' => $out]);
    }
    if ($path === '/api/chat') {
        $rows = db()->query('SELECT nick,uid,role,ts,text FROM chat ORDER BY id DESC LIMIT 60')->fetchAll();
        $msgs = [];
        foreach (array_reverse($rows) as $r) {
            $msgs[] = ['n' => $r['nick'], 'uid' => (int)$r['uid'], 'role' => $r['role'],
                       't' => (int)$r['ts'] * 1000, 'text' => $r['text']];
        }
        out(['messages' => $msgs]);
    }
    if ($path === '/api/stats') {
        $total = (int)db()->query('SELECT COUNT(*) c FROM users')->fetch()['c'];
        $since = time() - 900;
        $online = (int)db()->query("SELECT COUNT(DISTINCT user_id) c FROM sessions WHERE last_seen>$since")->fetch()['c'];
        out(['users' => $total, 'online' => $online]);
    }
    if ($path === '/api/builds') {
        $rows = db()->query('SELECT version,title,text,ts FROM builds ORDER BY id DESC LIMIT 20')->fetchAll();
        $list = [];
        foreach ($rows as $r) {
            $list[] = ['version' => $r['version'], 'title' => $r['title'],
                       'text' => $r['text'], 'date' => md1($r['ts'])];
        }
        out(['builds' => $list]);
    }
    if ($path === '/api/market') {
        $tag = $_GET['tag'] ?? '';
        $q = trim((string)($_GET['q'] ?? ''));
        $tags = explode(',', MARKET_TAGS);
        $sql = 'SELECT l.id,l.title,l.server_tag,l.price,l.sales,l.created,u.nick,u.uid
                FROM listings l JOIN users u ON u.id=l.seller_id WHERE l.active=1';
        $args = [];
        if (in_array($tag, $tags, true)) { $sql .= ' AND l.server_tag=?'; $args[] = $tag; }
        if ($q !== '') { $sql .= ' AND l.title LIKE ?'; $args[] = '%' . substr($q, 0, 40) . '%'; }
        $sql .= ' ORDER BY l.id DESC LIMIT 60';
        $st = db()->prepare($sql);
        $st->execute($args);
        $list = [];
        foreach ($st->fetchAll() as $r) {
            $list[] = ['id' => (int)$r['id'], 'title' => $r['title'], 'tag' => $r['server_tag'],
                       'price' => (int)$r['price'], 'sales' => (int)$r['sales'],
                       'seller' => $r['nick'], 'sellerUid' => (int)$r['uid'], 'date' => md1($r['created'])];
        }
        out(['listings' => $list, 'tags' => $tags]);
    }
    if ($path === '/api/launcher') {
        $rows = db()->query('SELECT version,title,text,ts FROM builds ORDER BY id DESC LIMIT 3')->fetchAll();
        $builds = [];
        foreach ($rows as $r) {
            $builds[] = ['version' => $r['version'], 'title' => $r['title'],
                         'text' => $r['text'], 'date' => md1($r['ts'])];
        }
        out(['version' => $builds[0]['version'] ?? 'v1.0', 'builds' => $builds]);
    }
    if ($path === '/api/launcher/file') {
        $u = user_by_token(reqToken());
        if (!$u) out(['error' => 'auth'], 401);
        $plan = $u['plan'] ?: 'TRIAL';
        $txt = "ETHEREAL CLIENT LOADER v1.0\n===========================\n"
             . "Nick: {$u['nick']}\nUID: {$u['uid']}\nPlan: {$plan}\n\n"
             . "MC 1.21.11 | Fabric Loader 0.19.5 | Java 21\n\n"
             . "INSTALL:\n1. Install Java 21 and Fabric Loader 0.19.5 for MC 1.21.11\n"
             . "2. Put ethereal-1.0.jar into .minecraft/mods\n"
             . "3. Launch the game, enter your UID in the Ethereal menu: {$u['uid']}\n"
             . "4. RSHIFT in game opens ClickGUI\n";
        header('Content-Type: text/plain; charset=utf-8');
        header('Content-Disposition: attachment; filename="ethereal-loader-uid-' . $u['uid'] . '.txt"');
        echo $txt;
        exit;
    }
    if ($path === '/api/admin/overview') {
        $u = me_or_die();
        admin_or_die($u);
        $pdo = db();
        $users = $pdo->query('SELECT * FROM users ORDER BY uid')->fetchAll();
        $unused = (int)$pdo->query('SELECT COUNT(*) c FROM keys WHERE used_by IS NULL')->fetch()['c'];
        $now = time();
        $active = 0; $outUsers = [];
        foreach ($users as $x) {
            $left = max(0, (int)($x['sub_until'] ?? 0) - $now);
            if ($left > 0) $active++;
            $outUsers[] = ['key' => strtolower($x['nick']), 'name' => $x['nick'], 'uid' => (int)$x['uid'],
                           'role' => $x['role'], 'plan' => $x['plan'], 'leftMs' => $left * 1000];
        }
        out(['total' => count($outUsers), 'active' => $active, 'unusedKeys' => $unused, 'users' => $outUsers]);
    }
    if ($path === '/api/admin/users') {
        $u = me_or_die();
        admin_or_die($u);
        $pdo = db();
        $rows = $pdo->query('SELECT * FROM users ORDER BY uid')->fetchAll();
        $now = time();
        $active = 0; $users = [];
        foreach ($rows as $x) {
            $left = max(0, (int)($x['sub_until'] ?? 0) - $now);
            if ($left > 0) $active++;
            $users[] = ['id' => (int)$x['id'], 'uid' => (int)$x['uid'], 'name' => $x['nick'],
                'key' => strtolower($x['nick']),
                'email' => $x['email'] ?? '', 'role' => $x['role'], 'plan' => $x['plan'],
                'subUntil' => (int)($x['sub_until'] ?? 0) * 1000, 'leftMs' => $left * 1000,
                'hwid' => $x['hwid'] ?: '', 'balance' => (int)($x['balance'] ?? 0),
                'banned' => (int)($x['banned'] ?? 0), 'created' => (int)$x['created'] * 1000];
        }
        $hw = $pdo->query('SELECT * FROM banned_hwids ORDER BY created DESC LIMIT 200')->fetchAll();
        $hwids = [];
        foreach ($hw as $r) {
            $hwids[] = ['hwid' => $r['hwid'], 'by' => $r['by_nick'], 'note' => $r['note'] ?? '',
                        'date' => md1($r['created'])];
        }
        $unused = (int)$pdo->query('SELECT COUNT(*) c FROM keys WHERE used_by IS NULL')->fetch()['c'];
        out(['total' => count($users), 'active' => $active, 'unusedKeys' => $unused,
             'users' => $users, 'hwids' => $hwids]);
    }
    out(['error' => 'unknown'], 404);
}

/* ================= POST ================= */
if ($method !== 'POST') out(['error' => 'method'], 405);

if ($path === '/api/register') {
    $nick = trim((string)($data['nick'] ?? ''));
    $email = trim((string)($data['email'] ?? ''));
    $password = (string)($data['pass'] ?? '');
    if (!preg_match(NICK_RE, $nick)) out(['error' => 'Ник: 3–16 символов (A-Z, 0-9, _)']);
    if (strlen($password) < 4) out(['error' => 'Пароль: минимум 4 символа']);
    if (!preg_match(EMAIL_RE, $email)) out(['error' => 'Введи корректную почту — она нужна для восстановления']);
    $pdo = db();
    $st = $pdo->prepare('SELECT 1 FROM users WHERE LOWER(nick)=?');
    $st->execute([strtolower($nick)]);
    if ($st->fetch()) out(['error' => 'Этот ник уже занят']);
    $st = $pdo->prepare('SELECT 1 FROM users WHERE LOWER(email)=?');
    $st->execute([strtolower($email)]);
    if ($st->fetch()) out(['error' => 'Эта почта уже привязана к другому аккаунту']);
    $counter = (int)$pdo->query("SELECT v FROM meta WHERE k='uid_counter'")->fetch()['v'];
    $role = strtolower($nick) === 'zxs' ? 'admin' : 'user';
    $pdo->prepare('INSERT INTO users(uid,nick,email,pass,role,hwid,created,sub_until,plan,trial_used,balance)
                   VALUES(?,?,?,?,?,NULL,?,0,NULL,0,0)')
        ->execute([$counter, $nick, $email, hash_pass($password), $role, time()]);
    $userId = (int)$pdo->lastInsertId();
    $pdo->prepare("UPDATE meta SET v=? WHERE k='uid_counter'")->execute([(string)($counter + 1)]);
    $token = bin2hex(random_bytes(24));
    $pdo->prepare('INSERT INTO sessions(token,user_id,created,last_seen) VALUES(?,?,?,?)')
        ->execute([$token, $userId, time(), time()]);
    add_log($pdo, $userId, 'аккаунт создан, UID ' . $counter);
    $st = $pdo->prepare('SELECT * FROM users WHERE id=?');
    $st->execute([$userId]);
    out(['token' => $token, 'user' => public_user($st->fetch())]);
}

if ($path === '/api/login') {
    $nick = trim((string)($data['nick'] ?? ''));
    $st = db()->prepare('SELECT * FROM users WHERE LOWER(nick)=?');
    $st->execute([strtolower($nick)]);
    $u = $st->fetch();
    if (!$u || !check_pass((string)($data['pass'] ?? ''), $u['pass']))
        out(['error' => 'Неверный ник или пароль']);
    if ((int)($u['banned'] ?? 0))
        out(['error' => 'Аккаунт заблокирован администратором. Напиши в поддержку.'], 403);
    $token = bin2hex(random_bytes(24));
    $pdo = db();
    $pdo->prepare('INSERT INTO sessions(token,user_id,created,last_seen) VALUES(?,?,?,?)')
        ->execute([$token, $u['id'], time(), time()]);
    add_log($pdo, (int)$u['id'], 'вход в кабинет');
    out(['token' => $token, 'user' => public_user($u)]);
}

if ($path === '/api/recover') {
    $nick = trim((string)($data['nick'] ?? ''));
    $email = trim((string)($data['email'] ?? ''));
    $newpass = (string)($data['newpass'] ?? '');
    if (strlen($newpass) < 4) out(['error' => 'Новый пароль: минимум 4 символа']);
    $st = db()->prepare('SELECT * FROM users WHERE LOWER(nick)=?');
    $st->execute([strtolower($nick)]);
    $u = $st->fetch();
    if (!$u || ($u['email'] ?? '') === '' || strtolower($u['email']) !== strtolower($email))
        out(['error' => 'Пара ник + почта не найдена. Проверь данные.']);
    $pdo = db();
    $pdo->prepare('UPDATE users SET pass=? WHERE id=?')->execute([hash_pass($newpass), $u['id']]);
    $pdo->prepare('DELETE FROM sessions WHERE user_id=?')->execute([$u['id']]);
    add_log($pdo, (int)$u['id'], 'пароль восстановлен через почту');
    out(['ok' => true]);
}

if ($path === '/api/logout') {
    db()->prepare('DELETE FROM sessions WHERE token=?')->execute([reqToken()]);
    out(['ok' => true]);
}

$u = me_or_die();

if ($path === '/api/activate') {
    $code = strtoupper(trim((string)($data['key'] ?? '')));
    if (!preg_match('/^ETHR-[A-Z0-9]{4}-[A-Z0-9]{4}$/', $code))
        out(['error' => 'Неверный формат. Пример: ETHR-AB12-CD34']);
    $pdo = db();
    $st = $pdo->prepare('SELECT * FROM keys WHERE code=?');
    $st->execute([$code]);
    $k = $st->fetch();
    if (!$k) out(['error' => 'Ключ не найден. Проверь код или купи тариф.']);
    if ($k['used_by']) out(['error' => 'Этот ключ уже использован']);
    $days = PLANS[$k['plan']] ?? 30;
    $base = max(time(), (int)($u['sub_until'] ?? 0));
    $pdo->prepare('UPDATE users SET sub_until=?, plan=? WHERE id=?')
        ->execute([$base + $days * DAY, $k['plan'], $u['id']]);
    $pdo->prepare('UPDATE keys SET used_by=? WHERE code=?')->execute([$u['uid'], $code]);
    add_log($pdo, (int)$u['id'], "ключ {$code} активирован → {$k['plan']} (+{$days} дн.)");
    out(['plan' => $k['plan'], 'days' => $days]);
}

if ($path === '/api/buy') {
    $plan = (string)($data['plan'] ?? '');
    $pay = (string)($data['pay'] ?? 'card');
    $card_hash = (string)($data['card_hash'] ?? '');
    if (!in_array($plan, BUYABLE, true)) out(['error' => 'Нет такого тарифа']);
    $price = PRICES[$plan];
    $pdo = db();
    if ($pay === 'balance') {
        $st = $pdo->prepare('SELECT balance FROM users WHERE id=?');
        $st->execute([$u['id']]);
        $bal = (int)$st->fetch()['balance'];
        if ($bal < $price) out(['error' => 'Не хватает ' . ($price - $bal) . ' ₽ на балансе. Пополни в кабинете.']);
        $pdo->prepare('UPDATE users SET balance=balance-? WHERE id=?')->execute([$price, $u['id']]);
        add_tx($pdo, (int)$u['id'], 'sub_buy', -$price, 'подписка ' . $plan);
    } else {
        $err = bind_card($card_hash, $u);
        if ($err) out(['error' => $err]);
    }
    $code = gen_key_code();
    $pdo->prepare('INSERT INTO keys(code,plan,created,used_by) VALUES(?,?,?,NULL)')
        ->execute([$code, $plan, time()]);
    if ($pay === 'balance') add_log($pdo, (int)$u['id'], "оплата {$plan} с баланса, выдан ключ {$code}");
    else add_log($pdo, (int)$u['id'], "оплата {$plan}, выдан ключ {$code} (демо)");
    out(['code' => $code, 'plan' => $plan]);
}

if ($path === '/api/billing') {
    $pdo = db();
    $st = $pdo->prepare('SELECT balance FROM users WHERE id=?');
    $st->execute([$u['id']]);
    $bal = (int)$st->fetch()['balance'];
    $st = $pdo->prepare('SELECT ts,kind,amount,text FROM transactions WHERE user_id=? ORDER BY id DESC LIMIT 30');
    $st->execute([$u['id']]);
    $txs = [];
    foreach ($st->fetchAll() as $r) {
        $txs[] = ['date' => mdtx($r['ts']), 'kind' => $r['kind'],
                  'amount' => (int)$r['amount'], 'text' => $r['text']];
    }
    out(['balance' => $bal, 'txs' => $txs]);
}

if ($path === '/api/topup') {
    $amount = (int)($data['amount'] ?? 0);
    if ($amount < 50 || $amount > 100000) out(['error' => 'Сумма от 50 до 100000 ₽']);
    $err = bind_card((string)($data['card_hash'] ?? ''), $u);
    if ($err) out(['error' => $err]);
    $pdo = db();
    $pdo->prepare('UPDATE users SET balance=COALESCE(balance,0)+? WHERE id=?')->execute([$amount, $u['id']]);
    add_tx($pdo, (int)$u['id'], 'topup', $amount, 'пополнение баланса');
    add_log($pdo, (int)$u['id'], "баланс пополнен на {$amount} ₽");
    $st = $pdo->prepare('SELECT balance FROM users WHERE id=?');
    $st->execute([$u['id']]);
    out(['balance' => (int)$st->fetch()['balance']]);
}

if ($path === '/api/hwid-reset') {
    db()->prepare('UPDATE users SET hwid=NULL WHERE id=?')->execute([$u['id']]);
    add_log(db(), (int)$u['id'], 'привязка HWID сброшена (оплата 150₽), привяжется при следующем запуске');
    out(['hwid' => 'bound-on-next-launch']);
}

if ($path === '/api/chat') {
    $text = usub(trim((string)($data['text'] ?? '')), 500);
    if ($text === '') out(['error' => 'Пустое сообщение']);
    db()->prepare('INSERT INTO chat(nick,uid,role,ts,text) VALUES(?,?,?,?,?)')
        ->execute([$u['nick'], $u['uid'], $u['role'], time(), $text]);
    out(['ok' => true]);
}

if ($path === '/api/client-launch') {
    $hwid = strtoupper(trim((string)($data['hwid'] ?? '')));
    if (!preg_match('/^[A-Z0-9]{4}-[A-Z0-9]{4}-[A-Z0-9]{4}$/', $hwid))
        out(['error' => 'Лаунчер передал пустой HWID']);
    $pdo = db();
    if ((int)($u['banned'] ?? 0))
        out(['error' => 'Аккаунт заблокирован администратором'], 403);
    $st = $pdo->prepare('SELECT 1 FROM banned_hwids WHERE hwid=?');
    $st->execute([$hwid]);
    if ($st->fetch())
        out(['error' => 'Это устройство заблокировано'], 403);
    $st = $pdo->prepare('SELECT hwid FROM users WHERE id=?');
    $st->execute([$u['id']]);
    $cur = $st->fetch()['hwid'];
    if (!$cur) {
        $pdo->prepare('UPDATE users SET hwid=? WHERE id=?')->execute([$hwid, $u['id']]);
        add_log($pdo, (int)$u['id'], 'железо привязано при запуске клиента: ' . $hwid);
        out(['bound' => true, 'hwid' => $hwid]);
    }
    if ($cur !== $hwid)
        out(['error' => 'Другое железо. Сбрось HWID в кабинете (150₽) и запускай снова.']);
    add_log($pdo, (int)$u['id'], 'запуск клиента подтверждён');
    out(['bound' => false, 'hwid' => $hwid]);
}

/* ---------- market ---------- */
if ($path === '/api/market/create') {
    $title = trim((string)($data['title'] ?? ''));
    $descr = trim((string)($data['descr'] ?? ''));
    $config = trim((string)($data['config'] ?? ''));
    $tag = (string)($data['tag'] ?? '');
    $price = (int)($data['price'] ?? 0);
    if (strlen($title) < 4 || strlen($title) > 80) out(['error' => 'Название: 4–80 символов']);
    if (!in_array($tag, explode(',', MARKET_TAGS), true)) out(['error' => 'Выбери сервер из списка']);
    if ($price < 10 || $price > 100000) out(['error' => 'Цена: 10–100000 ₽']);
    if (strlen($descr) < 10 || strlen($descr) > 2000) out(['error' => 'Описание: 10–2000 символов']);
    if (strlen($config) < 10 || strlen($config) > 20000) out(['error' => 'Вставь текст конфига (10–20000 символов)']);
    $pdo = db();
    $pdo->prepare('INSERT INTO listings(seller_id,title,server_tag,price,descr,config,created,active,sales)
                   VALUES(?,?,?,?,?,?,?,1,0)')
        ->execute([$u['id'], $title, $tag, $price, $descr, $config, time()]);
    $lid = (int)$pdo->lastInsertId();
    add_log($pdo, (int)$u['id'], "лот #{$lid} выставлен на маркет ({$price} ₽)");
    out(['id' => $lid]);
}
if ($path === '/api/market/buy') {
    $lid = (int)($data['id'] ?? 0);
    $pdo = db();
    $st = $pdo->prepare('SELECT * FROM listings WHERE id=?');
    $st->execute([$lid]);
    $l = $st->fetch();
    if (!$l || !(int)$l['active']) out(['error' => 'Лот снят с продажи']);
    if ((int)$l['seller_id'] === (int)$u['id']) out(['error' => 'Это твой собственный лот']);
    $st = $pdo->prepare('SELECT 1 FROM purchases WHERE buyer_id=? AND listing_id=?');
    $st->execute([$u['id'], $lid]);
    if ($st->fetch()) out(['error' => 'Ты уже купил этот конфиг — он во вкладке «Купленные»']);
    $st = $pdo->prepare('SELECT balance FROM users WHERE id=?');
    $st->execute([$u['id']]);
    $bal = (int)$st->fetch()['balance'];
    $price = (int)$l['price'];
    if ($bal < $price) out(['error' => 'Не хватает ' . ($price - $bal) . ' ₽ на балансе. Пополни в кабинете.']);
    $sellerCut = intdiv($price * (100 - MARKET_FEE), 100);
    $pdo->prepare('UPDATE users SET balance=balance-? WHERE id=?')->execute([$price, $u['id']]);
    $pdo->prepare('UPDATE users SET balance=COALESCE(balance,0)+? WHERE id=?')->execute([$sellerCut, $l['seller_id']]);
    $pdo->prepare('INSERT INTO purchases(buyer_id,listing_id,price,ts) VALUES(?,?,?,?)')
        ->execute([$u['id'], $lid, $price, time()]);
    $pdo->prepare('UPDATE listings SET sales=sales+1 WHERE id=?')->execute([$lid]);
    $st = $pdo->prepare('SELECT nick FROM users WHERE id=?');
    $st->execute([$l['seller_id']]);
    $seller = $st->fetch();
    $t40 = usub($l['title'], 40);
    add_tx($pdo, (int)$u['id'], 'market_buy', -$price, 'конфиг «' . $t40 . '»');
    add_tx($pdo, (int)$l['seller_id'], 'market_sale', $sellerCut,
           'продажа «' . $t40 . '» (комиссия ' . MARKET_FEE . '%)');
    add_log($pdo, (int)$u['id'], "куплен конфиг «{$t40}» за {$price} ₽");
    add_log($pdo, (int)$l['seller_id'], "продан конфиг «{$t40}» (+{$sellerCut} ₽)");
    out(['title' => $l['title'], 'config' => $l['config'],
         'seller' => $seller ? $seller['nick'] : '?']);
}
if ($path === '/api/market/my') {
    $pdo = db();
    $st = $pdo->prepare('SELECT id,title,server_tag,price,sales,active,created FROM listings WHERE seller_id=? ORDER BY id DESC');
    $st->execute([$u['id']]);
    $mine = [];
    foreach ($st->fetchAll() as $r) {
        $mine[] = ['id' => (int)$r['id'], 'title' => $r['title'], 'tag' => $r['server_tag'],
                   'price' => (int)$r['price'], 'sales' => (int)$r['sales'],
                   'active' => (bool)(int)$r['active'], 'date' => md1($r['created'])];
    }
    $st = $pdo->prepare('SELECT l.id,l.title,l.server_tag,p.price,p.ts,u.nick FROM purchases p
        JOIN listings l ON l.id=p.listing_id JOIN users u ON u.id=l.seller_id
        WHERE p.buyer_id=? ORDER BY p.ts DESC');
    $st->execute([$u['id']]);
    $bought = [];
    foreach ($st->fetchAll() as $r) {
        $bought[] = ['id' => (int)$r['id'], 'title' => $r['title'], 'tag' => $r['server_tag'],
                     'price' => (int)$r['price'], 'seller' => $r['nick'], 'date' => md1($r['ts'])];
    }
    out(['mine' => $mine, 'bought' => $bought]);
}
if ($path === '/api/market/config') {
    $lid = (int)($data['id'] ?? 0);
    $st = db()->prepare('SELECT l.title,l.config,l.server_tag FROM listings l
        LEFT JOIN purchases p ON p.listing_id=l.id AND p.buyer_id=?
        WHERE l.id=? AND (p.buyer_id IS NOT NULL OR l.seller_id=?)');
    $st->execute([$u['id'], $lid, $u['id']]);
    $r = $st->fetch();
    if (!$r) out(['error' => 'Сначала купи этот конфиг']);
    out(['title' => $r['title'], 'tag' => $r['server_tag'], 'config' => $r['config']]);
}
if ($path === '/api/market/toggle') {
    $lid = (int)($data['id'] ?? 0);
    $active = !empty($data['active']) ? 1 : 0;
    $pdo = db();
    $st = $pdo->prepare('SELECT id FROM listings WHERE id=? AND seller_id=?');
    $st->execute([$lid, $u['id']]);
    if (!$st->fetch()) out(['error' => 'Не твой лот']);
    $pdo->prepare('UPDATE listings SET active=? WHERE id=?')->execute([$active, $lid]);
    add_log($pdo, (int)$u['id'], 'лот #' . $lid . ($active ? ' возвращён на витрину' : ' снят с продажи'));
    out(['ok' => true]);
}

/* ---------- admin ---------- */
if ($path === '/api/admin/key') {
    admin_or_die($u);
    $plan = (string)($data['plan'] ?? '');
    if (!in_array($plan, BUYABLE, true)) out(['error' => 'Нет такого тарифа']);
    $code = gen_key_code();
    db()->prepare('INSERT INTO keys(code,plan,created,used_by) VALUES(?,?,?,NULL)')
        ->execute([$code, $plan, time()]);
    out(['code' => $code]);
}
if ($path === '/api/admin/role') {
    admin_or_die($u);
    $nick = (string)($data['nick'] ?? '');
    $role = (string)($data['role'] ?? '');
    if (!in_array($role, ROLES, true)) out(['error' => 'Неизвестная роль']);
    $pdo = db();
    $st = $pdo->prepare('SELECT * FROM users WHERE LOWER(nick)=?');
    $st->execute([strtolower($nick)]);
    $x = $st->fetch();
    if (!$x) out(['error' => 'Пользователь не найден']);
    if ((int)$x['id'] === (int)$u['id'] && $role !== 'admin') out(['error' => 'Нельзя снять роль с себя']);
    if ($role === 'admin' && strtolower($x['nick']) !== 'zxs') out(['error' => 'Админ может быть только zxs']);
    $pdo->prepare('UPDATE users SET role=? WHERE id=?')->execute([$role, $x['id']]);
    add_log($pdo, (int)$u['id'], 'роль ' . $x['nick'] . ' → ' . $role);
    out(['ok' => true]);
}
if ($path === '/api/admin/delete') {
    admin_or_die($u);
    $nick = (string)($data['nick'] ?? '');
    $pdo = db();
    $st = $pdo->prepare('SELECT * FROM users WHERE LOWER(nick)=?');
    $st->execute([strtolower($nick)]);
    $x = $st->fetch();
    if (!$x) out(['error' => 'Пользователь не найден']);
    if ((int)$x['id'] === (int)$u['id']) out(['error' => 'Нельзя удалить себя']);
    $pdo->prepare('DELETE FROM sessions WHERE user_id=?')->execute([$x['id']]);
    $pdo->prepare('DELETE FROM logs WHERE user_id=?')->execute([$x['id']]);
    $pdo->prepare('DELETE FROM users WHERE id=?')->execute([$x['id']]);
    add_log($pdo, (int)$u['id'], 'удалён пользователь ' . $x['nick']);
    out(['ok' => true]);
}
if ($path === '/api/admin/build') {
    admin_or_die($u);
    $version = usub(trim((string)($data['version'] ?? '')), 16);
    $title = usub(trim((string)($data['title'] ?? '')), 120);
    $text = usub(trim((string)($data['text'] ?? '')), 2000);
    if ($version === '' || $title === '' || $text === '')
        out(['error' => 'Заполни версию, заголовок и текст']);
    db()->prepare('INSERT INTO builds(version,title,text,ts) VALUES(?,?,?,?)')
        ->execute([$version, $title, $text, time()]);
    out(['ok' => true]);
}
if ($path === '/api/admin/find') {
    admin_or_die($u);
    $nick = strtolower(trim((string)($data['nick'] ?? '')));
    $st = db()->prepare('SELECT * FROM users WHERE LOWER(nick)=?');
    $st->execute([$nick]);
    $x = $st->fetch();
    if (!$x) out(['error' => 'Пользователь не найден']);
    $now = time();
    $left = max(0, (int)($x['sub_until'] ?? 0) - $now);
    out(['user' => ['id' => (int)$x['id'], 'uid' => (int)$x['uid'], 'name' => $x['nick'],
        'role' => $x['role'], 'plan' => $x['plan'], 'leftMs' => $left * 1000,
        'hwid' => $x['hwid'] ?: '', 'balance' => (int)($x['balance'] ?? 0),
        'banned' => (int)($x['banned'] ?? 0)]]);
}
if ($path === '/api/admin/ban') {
    admin_or_die($u);
    $nick = strtolower(trim((string)($data['nick'] ?? '')));
    $banned = !empty($data['banned']) ? 1 : 0;
    $pdo = db();
    $st = $pdo->prepare('SELECT * FROM users WHERE LOWER(nick)=?');
    $st->execute([$nick]);
    $x = $st->fetch();
    if (!$x) out(['error' => 'Пользователь не найден']);
    if ((int)$x['id'] === (int)$u['id']) out(['error' => 'Нельзя забанить себя']);
    if ($banned && ($x['role'] ?? '') === 'admin') out(['error' => 'Нельзя забанить админа']);
    $pdo->prepare('UPDATE users SET banned=? WHERE id=?')->execute([$banned, $x['id']]);
    if ($banned) $pdo->prepare('DELETE FROM sessions WHERE user_id=?')->execute([$x['id']]);
    add_log($pdo, (int)$x['id'], $banned ? 'аккаунт заблокирован (' . $u['nick'] . ')' : 'аккаунт разблокирован (' . $u['nick'] . ')');
    out(['ok' => true, 'banned' => $banned]);
}
if ($path === '/api/admin/ban-hwid') {
    admin_or_die($u);
    $hwid = strtoupper(trim((string)($data['hwid'] ?? '')));
    $banned = !empty($data['banned']);
    if (!preg_match('/^[A-Z0-9]{4}-[A-Z0-9]{4}-[A-Z0-9]{4}$/', $hwid))
        out(['error' => 'Формат HWID: XXXX-XXXX-XXXX']);
    $pdo = db();
    if ($banned) {
        $st = $pdo->prepare('INSERT OR IGNORE INTO banned_hwids(hwid,by_nick,note,created) VALUES(?,?,?,?)');
        if (my()) {
            $st = $pdo->prepare('INSERT INTO banned_hwids(hwid,by_nick,note,created) VALUES(?,?,?,?)
                                 ON DUPLICATE KEY UPDATE by_nick=VALUES(by_nick)');
        }
        $st->execute([$hwid, $u['nick'], (string)($data['note'] ?? ''), time()]);
        $st = $pdo->prepare('SELECT id,nick FROM users WHERE hwid=?');
        $st->execute([$hwid]);
        foreach ($st->fetchAll() as $owner) {
            add_log($pdo, (int)$owner['id'], 'устройство ' . $hwid . ' заблокировано (' . $u['nick'] . ')');
        }
    } else {
        $pdo->prepare('DELETE FROM banned_hwids WHERE hwid=?')->execute([$hwid]);
    }
    out(['ok' => true, 'banned' => $banned ? 1 : 0]);
}
if ($path === '/api/admin/sub') {
    admin_or_die($u);
    $nick = strtolower(trim((string)($data['nick'] ?? '')));
    $days = (int)($data['days'] ?? 0);
    if ($days < 0 || $days > 3650) out(['error' => 'Дни: от 0 до 3650 (0 = снять подписку)']);
    $pdo = db();
    $st = $pdo->prepare('SELECT * FROM users WHERE LOWER(nick)=?');
    $st->execute([$nick]);
    $x = $st->fetch();
    if (!$x) out(['error' => 'Пользователь не найден']);
    if ($days === 0) {
        $pdo->prepare('UPDATE users SET sub_until=?, plan=NULL WHERE id=?')->execute([time(), $x['id']]);
        add_log($pdo, (int)$x['id'], 'подписка снята (' . $u['nick'] . ')');
        out(['ok' => true, 'subUntil' => time() * 1000, 'plan' => null]);
    }
    $base = max(time(), (int)($x['sub_until'] ?? 0));
    $until = $base + $days * DAY;
    $plan = $x['plan'] ?: 'D30';
    $pdo->prepare('UPDATE users SET sub_until=?, plan=? WHERE id=?')->execute([$until, $plan, $x['id']]);
    add_log($pdo, (int)$x['id'], "подписка продлена +{$days} дн. → до " . date('d.m.Y', $until) . ' (' . $u['nick'] . ')');
    out(['ok' => true, 'subUntil' => $until * 1000, 'plan' => $plan]);
}
if ($path === '/api/admin/balance') {
    admin_or_die($u);
    $nick = strtolower(trim((string)($data['nick'] ?? '')));
    $amount = (int)($data['amount'] ?? 0);
    if ($amount === 0 || $amount < -1000000 || $amount > 1000000)
        out(['error' => 'Сумма: от -1000000 до 1000000, не 0']);
    $pdo = db();
    $st = $pdo->prepare('SELECT * FROM users WHERE LOWER(nick)=?');
    $st->execute([$nick]);
    $x = $st->fetch();
    if (!$x) out(['error' => 'Пользователь не найден']);
    $pdo->prepare('UPDATE users SET balance=COALESCE(balance,0)+? WHERE id=?')->execute([$amount, $x['id']]);
    add_tx($pdo, (int)$x['id'], 'admin_adjust', $amount, 'корректировка баланса (' . $u['nick'] . ')');
    add_log($pdo, (int)$x['id'], 'баланс ' . ($amount > 0 ? '+' : '') . $amount . ' ₽ (' . $u['nick'] . ')');
    $st = $pdo->prepare('SELECT balance FROM users WHERE id=?');
    $st->execute([$x['id']]);
    out(['ok' => true, 'balance' => (int)$st->fetch()['balance']]);
}
if ($path === '/api/admin/hwid-reset') {
    admin_or_die($u);
    $nick = strtolower(trim((string)($data['nick'] ?? '')));
    $pdo = db();
    $st = $pdo->prepare('SELECT * FROM users WHERE LOWER(nick)=?');
    $st->execute([$nick]);
    $x = $st->fetch();
    if (!$x) out(['error' => 'Пользователь не найден']);
    $pdo->prepare('UPDATE users SET hwid=NULL WHERE id=?')->execute([$x['id']]);
    add_log($pdo, (int)$x['id'], 'привязка HWID сброшена администратором (' . $u['nick'] . ')');
    out(['ok' => true]);
}

out(['error' => 'unknown'], 404);
