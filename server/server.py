"""Etherial — static site + JSON API. Standard library only.

Run:  python server/server.py   (or start.bat)
Open: http://localhost:8000
"""
import json
import mimetypes
import os
import re
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse

import db

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PORT = 8000
NICK_RE = re.compile(r'^[A-Za-z0-9_]{3,16}$')


class Handler(BaseHTTPRequestHandler):
    server_version = 'Etherial/1.0'

    def handle_expect_100(self):
        self.send_response_only(100)
        self.end_headers()
        return True

    def end_headers(self):
        self.send_header('Access-Control-Allow-Origin', '*');
        self.send_header('Access-Control-Allow-Headers', 'Content-Type, Authorization')
        self.send_header('Access-Control-Allow-Methods', 'GET, POST, OPTIONS')
        super().end_headers()

    def do_OPTIONS(self):
        self.send_response(204)
        self.end_headers()

    # ---------- helpers ----------

    def _json(self, obj, code=200):
        body = json.dumps(obj, ensure_ascii=False).encode('utf-8')
        self.send_response(code)
        self.send_header('Content-Type', 'application/json; charset=utf-8')
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _body(self):
        try:
            length = int(self.headers.get('Content-Length') or 0)
        except ValueError:
            length = 0
        if length <= 0 or length > 65536:
            return {}
        try:
            return json.loads(self.rfile.read(length).decode('utf-8'))
        except (ValueError, UnicodeDecodeError):
            return {}

    def _token(self):
        auth = self.headers.get('Authorization') or ''
        if auth.startswith('Bearer '):
            return auth[7:].strip()
        return ''

    def _me(self):
        return db.get_user_by_token(self._token())

    # ---------- static ----------

    def _serve_static(self, path):
        if path in ('/', ''):
            path = '/index.html'
        safe = path.replace('\\', '/').lstrip('/')
        parts = [p for p in safe.split('/') if p not in ('', '.')]
        if not parts or '..' in parts:
            self.send_error(403)
            return
        full = os.path.join(ROOT, *parts)
        if not os.path.abspath(full).startswith(os.path.abspath(ROOT)):
            self.send_error(403)
            return
        if os.path.isdir(full):
            full = os.path.join(full, 'index.html')
        if not os.path.isfile(full):
            self.send_error(404)
            return
        mime, _ = mimetypes.guess_type(full)
        with open(full, 'rb') as f:
            body = f.read()
        self.send_response(200)
        self.send_header('Content-Type', mime or 'application/octet-stream')
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        path = urlparse(self.path).path
        if path == '/api/me':
            u = self._me()
            if not u:
                return self._json({'error': 'auth'})
            out = db.public_user(u)
            out['log'] = db.user_logs(u['id'])
            return self._json({'user': out})
        if path == '/api/chat':
            return self._json({'messages': db.get_chat()})
        if path == '/api/irc/poll':
            u = self._me()
            if not u:
                return self._json({'error': 'auth'}, 401)
            import urllib.parse as _up
            q = _up.parse_qs(urlparse(self.path).query)
            try:
                g_id = int((q.get('g') or ['0'])[0])
            except ValueError:
                g_id = 0
            try:
                d_id = int((q.get('d') or ['0'])[0])
            except ValueError:
                d_id = 0
            return self._json(db.irc_poll(u, g_id, d_id))
        if path == '/api/irc/dm':
            u = self._me()
            if not u:
                return self._json({'error': 'auth'}, 401)
            import urllib.parse as _up
            q = _up.parse_qs(urlparse(self.path).query)
            other = (q.get('with') or [''])[0]
            msgs = db.irc_dm_history(u, other)
            if msgs is None:
                return self._json({'error': 'Игрок не найден'})
            return self._json({'messages': msgs})
        if path == '/api/stats':
            return self._json(db.stats())
        if path == '/api/builds':
            return self._json({'builds': db.get_builds()})
        if path == '/api/market':
            import urllib.parse as _up
            q = _up.parse_qs(urlparse(self.path).query)
            tag = (q.get('tag') or [''])[0]
            qq = (q.get('q') or [''])[0]
            return self._json({'listings': db.market_list(tag, qq), 'tags': db.MARKET_TAGS})
        if path == '/api/launcher':
            builds = db.get_builds()[:3]
            return self._json({'version': builds[0]['version'] if builds else 'v1.0',
                               'builds': builds})
        if path == '/api/launcher/file':
            lu = self._me()
            if not lu:
                return self._json({'error': 'auth'}, 401)
            plan = lu['plan'] or 'TRIAL'
            txt = ('ETHERIAL CLIENT LOADER v1.0\n===========================\n'
                   'Nick: %s\nUID: %d\nPlan: %s\n\nMC 1.21.11 | Fabric Loader 0.19.5 | Java 21\n\n'
                   'INSTALL:\n1. Install Java 21 and Fabric Loader 0.19.5 for MC 1.21.11\n'
                   '2. Put etherial-1.0.jar into .minecraft/mods\n'
                   '3. Launch the game, enter your UID in the Etherial menu: %d\n'
                   '4. RSHIFT in game opens ClickGUI\n' % (lu['nick'], lu['uid'], plan, lu['uid']))
            body = txt.encode('utf-8')
            self.send_response(200)
            self.send_header('Content-Type', 'text/plain; charset=utf-8')
            self.send_header('Content-Disposition',
                             'attachment; filename="etherial-loader-uid-%d.txt"' % lu['uid'])
            self.send_header('Content-Length', str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return
        if path == '/api/admin/overview':
            u = self._me()
            if not u or u['role'] != 'admin':
                return self._json({'error': 'Только для админа'}, 403)
            return self._json(db.admin_overview())
        return self._serve_static(path)

    def do_POST(self):
        path = urlparse(self.path).path
        data = self._body()

        if path == '/api/register':
            nick = (data.get('nick') or '').strip()
            email = (data.get('email') or '').strip()
            password = data.get('pass') or ''
            if not NICK_RE.match(nick):
                return self._json({'error': 'Ник: 3–16 символов (A-Z, 0-9, _)'})
            if len(password) < 4:
                return self._json({'error': 'Пароль: минимум 4 символа'})
            res, err = db.register(nick, email, password)
            if err:
                return self._json({'error': err})
            return self._json(res)

        if path == '/api/login':
            res, err = db.login((data.get('nick') or '').strip(), data.get('pass') or '')
            if err:
                return self._json({'error': err})
            return self._json(res)

        if path == '/api/recover':
            err = db.recover(data.get('nick') or '', data.get('email') or '',
                             data.get('newpass') or '')
            if err:
                return self._json({'error': err})
            return self._json({'ok': True})

        if path == '/api/logout':
            db.logout(self._token())
            return self._json({'ok': True})

        u = self._me()
        if not u:
            return self._json({'error': 'auth'}, 401)

        if path == '/api/activate':
            res, err = db.activate(u, data.get('key'))
            if err:
                return self._json({'error': err})
            return self._json(res)

        if path == '/api/buy':
            res, err = db.buy_plan(u, data.get('plan'), data.get('pay') or 'card',
                                   data.get('card_hash') or '')
            if err:
                return self._json({'error': err})
            return self._json(res)

        if path == '/api/billing':
            return self._json(db.billing(u))

        if path == '/api/topup':
            res, err = db.topup(u, data.get('amount'), data.get('card_hash') or '')
            if err:
                return self._json({'error': err})
            return self._json(res)

        if path == '/api/hwid-reset':
            hwid = db.reset_hwid(u)
            return self._json({'hwid': hwid})

        if path == '/api/chat':
            if db.post_chat(u, data.get('text')):
                return self._json({'ok': True})
            return self._json({'error': 'Пустое сообщение'})

        if path == '/api/irc/send':
            msg = db.post_chat(u, data.get('text'))
            if not msg:
                return self._json({'error': 'Пустое сообщение'})
            return self._json({'message': msg})

        if path == '/api/irc/dm':
            msg, err = db.irc_dm_send(u, data.get('to'), data.get('text'))
            if err:
                return self._json({'error': err})
            return self._json({'message': msg})

        if path == '/api/irc/friend/add':
            err = db.irc_friend_add(u, data.get('nick'))
            return self._json({'error': err} if err else {'ok': True})

        if path == '/api/irc/friend/accept':
            err = db.irc_friend_accept(u, data.get('nick'))
            return self._json({'error': err} if err else {'ok': True})

        if path == '/api/irc/friend/decline':
            err = db.irc_friend_decline(u, data.get('nick'))
            return self._json({'error': err} if err else {'ok': True})

        if path == '/api/irc/friend/remove':
            err = db.irc_friend_remove(u, data.get('nick'))
            return self._json({'error': err} if err else {'ok': True})

        if path == '/api/client-launch':
            res, err = db.client_launch(u, data.get('hwid'))
            if err:
                return self._json({'error': err})
            return self._json(res)

        if path == '/api/market/create':
            res, err = db.market_create(u, data.get('title'), data.get('tag'),
                                        data.get('price'), data.get('descr'), data.get('config'))
            if err:
                return self._json({'error': err})
            return self._json(res)

        if path == '/api/market/buy':
            res, err = db.market_buy(u, data.get('id'))
            if err:
                return self._json({'error': err})
            return self._json(res)

        if path == '/api/market/my':
            return self._json(db.market_my(u))

        if path == '/api/market/config':
            res, err = db.market_config(u, data.get('id'))
            if err:
                return self._json({'error': err})
            return self._json(res)

        if path == '/api/market/toggle':
            res, err = db.market_toggle(u, data.get('id'), data.get('active'))
            if err:
                return self._json({'error': err})
            return self._json(res)

        if path == '/api/admin/key':
            if u['role'] != 'admin':
                return self._json({'error': 'Только для админа'}, 403)
            if data.get('plan') not in db.BUYABLE:
                return self._json({'error': 'Нет такого тарифа'})
            return self._json({'code': db.mint_key(data['plan'])})

        if path == '/api/admin/role':
            if u['role'] != 'admin':
                return self._json({'error': 'Только для админа'}, 403)
            err = db.set_role(u, data.get('nick') or '', data.get('role') or '')
            if err:
                return self._json({'error': err})
            return self._json({'ok': True})

        if path == '/api/admin/delete':
            if u['role'] != 'admin':
                return self._json({'error': 'Только для админа'}, 403)
            err = db.delete_user(u, data.get('nick') or '')
            if err:
                return self._json({'error': err})
            return self._json({'ok': True})

        if path == '/api/admin/build':
            if u['role'] != 'admin':
                return self._json({'error': 'Только для админа'}, 403)
            err = db.add_build(data.get('version'), data.get('title'), data.get('text'))
            if err:
                return self._json({'error': err})
            return self._json({'ok': True})

        if path == '/api/admin/poll':
            if u['role'] != 'admin':
                return self._json({'error': 'Только для админа'}, 403)
            err = db.add_poll(data.get('question'), data.get('options'))
            if err:
                return self._json({'error': err})
            return self._json({'ok': True})

        return self._json({'error': 'unknown'}, 404)

    def log_message(self, fmt, *args):
        pass  # quiet


if __name__ == '__main__':
    import os as _os
    db.init()
    port = int(_os.environ.get('PORT', '8000'))
    srv = ThreadingHTTPServer(('0.0.0.0', port), Handler)
    print('Etherial server on port %d' % port, flush=True)
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        pass
