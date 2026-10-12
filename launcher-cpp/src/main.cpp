// Etherial C++ launcher: glue between custom UI, site API and game bootstrap.
#include "ui.h"
#include "mc.h"
#include "sys.h"
#include <shellapi.h>
#include <thread>
#include <cstdio>

static std::string g_token;
static sysutil::Config g_cfg;
static std::wstring g_hwid;
static ui::Ui* g_ui = nullptr;

namespace ui { std::wstring g_localHwid() { return g_hwid; } }

static std::string w2s(const std::wstring& w) {
    if (w.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string s(n - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &s[0], n, nullptr, nullptr);
    return s;
}
static void bg(std::function<void()> f) {
    std::thread([f] { f(); }).detach();
}
static http::Result apiPost(const std::string& path, const std::string& body) {
    std::wstring url = g_cfg.server;
    while (!url.empty() && url.back() == L'/') url.pop_back();
    return http::request(url + http::narrow_to_wide(path), body);
}
static http::Result apiGet(const std::string& path) {
    std::wstring url = g_cfg.server;
    while (!url.empty() && url.back() == L'/') url.pop_back();
    auto r = http::request(url + http::narrow_to_wide(path), "");
    return r;
}
static void setStatus(const std::wstring& s) {
    if (g_ui) { g_ui->statusLine = s; g_ui->invalidate(); }
}

// POST with Authorization header (our http::request lacks headers; do it here).
static http::Result apiAuthed(const std::string& path, const std::string& body) {
    http::Result r;
    std::wstring url = g_cfg.server;
    while (!url.empty() && url.back() == L'/') url.pop_back();
    HINTERNET inet = InternetOpenW(L"EtherialLauncher/1.0", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
    if (!inet) { r.error = L"no inet"; return r; }
    std::wstring full = url + http::narrow_to_wide(path);
    URL_COMPONENTSW p = { sizeof(p) };
    wchar_t host[256] = {}, reqpath[4096] = {};
    p.lpszHostName = host; p.dwHostNameLength = 256;
    p.lpszUrlPath = reqpath; p.dwUrlPathLength = 4096;
    p.dwSchemeLength = 32;
    if (!InternetCrackUrlW(full.c_str(), (DWORD)full.size(), 0, &p)) {
        InternetCloseHandle(inet); r.error = L"bad url"; return r;
    }
    HINTERNET conn = InternetConnectW(inet, host, p.nPort, nullptr, nullptr, INTERNET_SERVICE_HTTP, 0, 0);
    if (!conn) { InternetCloseHandle(inet); r.error = L"connect"; return r; }
    HINTERNET req = HttpOpenRequestW(conn, body.empty() ? L"GET" : L"POST", reqpath,
        nullptr, nullptr, nullptr,
        INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE
            | (p.nScheme == INTERNET_SCHEME_HTTPS ? INTERNET_FLAG_SECURE : 0), 0);
    if (req) {
        std::wstring hdrs = L"Content-Type: application/json\r\nAuthorization: Bearer "
            + http::narrow_to_wide(g_token) + L"\r\n";
        if (HttpSendRequestW(req, hdrs.c_str(), (DWORD)hdrs.size(),
                body.empty() ? nullptr : (LPVOID)body.data(), (DWORD)body.size())) {
            DWORD st = 0, sl = sizeof(st);
            HttpQueryInfoW(req, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &st, &sl, nullptr);
            r.code = (long)st;
            char buf[65536]; DWORD rd = 0;
            while (InternetReadFile(req, buf, sizeof(buf), &rd) && rd > 0)
                r.body.append(buf, rd);
        } else r.error = L"send";
        InternetCloseHandle(req);
    } else r.error = L"open";
    InternetCloseHandle(conn);
    InternetCloseHandle(inet);
    return r;
}

static void refreshMeNow() {
    auto r = apiAuthed("/api/me", "");
    if (r.code != 200) return;
    try {
        auto j = json::parse(r.body);
        auto u = j.at(L"user");
        if (u.type == json::Value::OBJ && g_ui) {
            g_ui->me = u;
            g_ui->hasMe = true;
            g_ui->screen = ui::Ui::Screen::MAIN;
            g_ui->invalidate();
        }
    } catch (...) {}
}

static std::string esc(const std::string& s) {
    std::string o;
    for (char c : s) {
        if (c == '"') o += "\\\"";
        else if (c == '\\') o += "\\\\";
        else if (c == '\n') o += "\\n";
        else o += c;
    }
    return o;
}

// Full pipeline: java -> game files -> HWID gate -> run.
static void pipeline(bool launchAfter) {
    if (!g_ui) return;
    g_ui->dlProgress = 0;
    mc::Progress pr;
    auto tick = [&] {
        if (g_ui) { g_ui->statusLine = pr.label; g_ui->dlProgress = pr.pct; g_ui->invalidate(); }
    };
    std::wstring err, javaw;
    mc::Game game;
    game.dir = g_cfg.gameDir;
    setStatus(L"Java…"); tick();
    if (!game.ensureJava(javaw, &pr, err)) { setStatus(L"ошибка: " + err); g_ui->dlProgress = -1; g_ui->toast(err, true); return; }
    tick();
    if (!game.prepare(&pr, err, mc::MOD_URL)) {
        setStatus(L"ошибка: " + err);
        if (g_ui) { g_ui->dlProgress = -1; g_ui->toast(err, true); }
        return;
    }
    // update mod info line
    {
        std::wstring mp = mc::join(mc::join(game.dir, L"mods"), L"etherial-client.jar");
        unsigned long long sz = mc::fsize(mp);
        if (g_ui) g_ui->jarInfo = std::to_wstring(sz / 1048576) + L" МБ · sha256 " +
            http::narrow_to_wide(crypto::fileSha256(mp).substr(0, 16)) + L"…";
    }
    if (g_ui) { g_ui->dlProgress = -1; g_ui->invalidate(); }
    setStatus(L"готово");
    if (!launchAfter) {
        if (g_ui) g_ui->toast(L"Файлы проверены", false);
        return;
    }
    // HWID gate: binds on first launch under own login
    setStatus(L"проверка HWID…");
    {
        std::string body = "{\"hwid\":\"" + w2s(g_hwid) + "\"}";
        auto r = apiAuthed("/api/client-launch", body);
        if (r.code != 200) { setStatus(L"отклонено сервером"); if (g_ui) g_ui->toast(L"Сервер недоступен", true); return; }
        json::Value j;
        try { j = json::parse(r.body); } catch (...) { if (g_ui) g_ui->toast(L"Ответ сервера", true); return; }
        if (j.at(L"error").type == json::Value::STR) {
            setStatus(L"отклонено");
            if (g_ui) g_ui->toast(j.at(L"error").s(), true);
            return;
        }
        if (j.at(L"bound").type == json::Value::BOOL && j.at(L"bound").b && g_ui)
            g_ui->toast(L"Железо привязано к твоему UID", false);
    }
    setStatus(L"запуск…");
    std::string nick = g_ui && g_ui->hasMe ? w2s(g_ui->me.at(L"name").s()) : "player";
    if (!game.launch(javaw, http::narrow_to_wide(nick), crypto::offlineUuid(nick),
                     g_ui ? g_ui->ramGb : 4, err)) {
        setStatus(L"ошибка запуска");
        if (g_ui) g_ui->toast(err, true);
        return;
    }
    setStatus(L"игра запущена");
    if (g_ui) {
        g_ui->lastLaunched = L"игра запущена";
        g_ui->toast(L"Клиент запущен", false);
    }
}

static int selftest(const std::string& base, const std::string& nick, const std::string& pass) {
    FILE* lf = fopen("C:\\Users\\ARTEMK~1\\AppData\\Local\\Temp\\opencode\\selftest.log", "w");
    auto say = [&](bool ok, const char* name, const std::string& extra = "") {
        char line[512];
        snprintf(line, sizeof(line), "%s %s %s", ok ? "ok  " : "FAIL", name, extra.c_str());
        printf("%s\n", line);
        if (lf) { fprintf(lf, "%s\n", line); fflush(lf); }
    };
    int fails = 0;
    auto check = [&](bool ok, const char* name) {
        say(ok, name);
        if (!ok) fails++;
    };
    // json
    try {
        auto v = json::parse("{\"a\":[1,\"x\\u00e9\",true,null],\"b\":{\"c\":-3}}");
        auto a = v.at(L"a").arr;
        check(a.size() == 4, "json arr");
        check(v.at(L"a").arr[1].s() == L"x\u00e9", "json unicode");
        check(v.at(L"b").at(L"c").i() == -3, "json nested");
    } catch (...) { check(false, "json parse"); }
    // redirect following: adoptium api (302) — header only check via http code path
    // https direct: mojang manifest (200, no redirect needed)
    {
        auto r = http::get(L"https://piston-meta.mojang.com/mc/game/version_manifest_v2.json");
        bool okh = r.code == 200 && r.body.find("\"versions\"") != std::string::npos;
        say(okh, "https direct", std::to_string(r.code));
        if (!okh) fails++;
    }
    {
        auto r = http::get(L"http://github.com");
        bool okr = r.code == 200;
        say(okr, "https redirect", std::to_string(r.code));
        if (!okr) fails++;
    }
    g_cfg.server = http::narrow_to_wide(base);
    // stats
    {
        auto r = apiGet("/api/stats");
        check(r.code == 200, "api stats");
    }
    if (!nick.empty()) {
        auto r = apiPost("/api/login", "{\"nick\":\"" + esc(nick) + "\",\"pass\":\"" + esc(pass) + "\"}");
        json::Value j;
        try { j = json::parse(r.body); } catch (...) {}
        bool okLogin = j.at(L"token").type == json::Value::STR;
        check(okLogin, "api login");
        if (okLogin) {
            g_token = w2s(j.at(L"token").s());
            auto m = apiAuthed("/api/me", "");
            json::Value mu;
            try { mu = json::parse(m.body); } catch (...) {}
            check(mu.at(L"user").at(L"name").type == json::Value::STR, "api me authed");
            // hwid gate dry-run with fake id (must NOT match real one -> expect reject or bind?)
            // use obviously-fake format-valid id; on fresh test db it would bind — skip mutating.
            printf("skip live hwid bind in selftest (mutates account)\n");
        }
        auto b = apiGet("/api/builds");
        json::Value bj;
        try { bj = json::parse(b.body); } catch (...) {}
        check(bj.at(L"builds").type == json::Value::ARR, "api builds");
    }
    // offline uuid format
    {
        std::string u = crypto::offlineUuid("testnick");
        check(u.size() == 36 && u[8] == '-' && u[14] == '3', "offline uuid");
    }
    say(fails == 0, fails == 0 ? "SELFTEST PASS" : "SELFTEST FAIL");
    if (lf) fclose(lf);
    return fails;
}

int main() {
    SetProcessDPIAware();
    g_hwid = http::narrow_to_wide(sysutil::machineId());
    return 0; // real entry below (wmain)
}

// GUI entry
int WINAPI wWinMain(HINSTANCE h, HINSTANCE, LPWSTR cmd, int) {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    for (int i = 1; i < argc; i++) {
        if (!lstrcmpW(argv[i], L"--selftest")) {
            AttachConsole(ATTACH_PARENT_PROCESS);
            freopen("CONOUT$", "w", stdout);
            std::string base = "http://cg839385.tw1.ru", nick, pass;
            if (i + 1 < argc) { char b[512] = {}; WideCharToMultiByte(CP_UTF8, 0, argv[i+1], -1, b, 512, nullptr, nullptr); base = b; }
            if (i + 3 < argc) {
                char b1[128] = {}, b2[128] = {};
                WideCharToMultiByte(CP_UTF8, 0, argv[i+2], -1, b1, 128, nullptr, nullptr);
                WideCharToMultiByte(CP_UTF8, 0, argv[i+3], -1, b2, 128, nullptr, nullptr);
                nick = b1; pass = b2;
            }
            return selftest(base, nick, pass);
        }
    }
    g_cfg.load();
    if (g_cfg.gameDir.empty()) {
        wchar_t* ad = nullptr; size_t n = 0;
        _wdupenv_s(&ad, &n, L"APPDATA");
        g_cfg.gameDir = std::wstring(ad ? ad : L".") + L"\\Etherial\\mc";
        if (ad) free(ad);
    }
    static ui::Ui ui;
    g_ui = &ui;
    ui.ramGb = g_cfg.ramGb;
    ui.serverF.set(g_cfg.server);
    if (!g_cfg.token.empty()) {
        // token restore happens via refresh below
    }

    ui.onLogin = [] {
        std::string nick = w2s(ui.nickF.value()), pass = w2s(ui.passF.value());
        if (nick.empty() || pass.empty()) { ui.toast(L"Введи ник и пароль", true); return; }
        std::thread([](std::string nick, std::string pass) {
            auto r = apiPost("/api/login", "{\"nick\":\"" + esc(nick) + "\",\"pass\":\"" + esc(pass) + "\"}");
            if (r.code != 200) { ui.toast(L"Нет связи", true); return; }
            json::Value j;
            try { j = json::parse(r.body); } catch (...) { ui.toast(L"Ответ сервера", true); return; }
            if (j.at(L"error").type == json::Value::STR) { ui.toast(j.at(L"error").s(), true); return; }
            g_token = w2s(j.at(L"token").s());
            g_cfg.server = ui.serverF.value().empty() ? L"http://cg839385.tw1.ru" : ui.serverF.value();
            g_cfg.token = g_token;
            g_cfg.ramGb = ui.ramGb;
            g_cfg.save();
            refreshMeNow();
        }, nick, pass).detach();
    };
    ui.onRecover = [] {
        std::string n = w2s(ui.recNickF.value()), e = w2s(ui.recMailF.value()), p = w2s(ui.recPassF.value());
        if (n.empty() || e.empty() || p.empty()) { ui.toast(L"Заполни все три поля", true); return; }
        std::thread([n, e, p] {
            auto r = apiPost("/api/recover",
                "{\"nick\":\"" + esc(n) + "\",\"email\":\"" + esc(e) + "\",\"newpass\":\"" + esc(p) + "\"}");
            json::Value j;
            try { j = json::parse(r.body); } catch (...) { ui.toast(L"Нет связи", true); return; }
            if (j.at(L"error").type == json::Value::STR) { ui.toast(j.at(L"error").s(), true); return; }
            ui.forgotMode = false;
            ui.toast(L"Пароль изменён, входи", false);
            ui.invalidate();
        }).detach();
    };
    ui.onLaunch = [] {
        std::thread([] { pipeline(true); }).detach();
    };
    ui.onDownload = [] {
        std::thread([] { pipeline(false); }).detach();
    };
    ui.onLogout = [] {
        std::thread([] {
            if (!g_token.empty()) apiAuthed("/api/logout", "{}");
            g_token.clear();
            g_cfg.token.clear();
            g_cfg.save();
            ui.me = json::Value::nul();
            ui.hasMe = false;
            ui.screen = ui::Ui::Screen::LOGIN;
            ui.invalidate();
        }).detach();
    };
    ui.onSaveSettings = [] {
        g_cfg.server = ui.serverF.value().empty() ? L"http://cg839385.tw1.ru" : ui.serverF.value();
        g_cfg.ramGb = ui.ramGb;
        g_cfg.save();
        ui.toast(L"Сохранено", false);
    };
    ui.onTestConn = [] {
        std::thread([] {
            auto r = apiGet("/api/stats");
            if (r.code != 200) { ui.toast(L"Нет связи", true); return; }
            try {
                auto j = json::parse(r.body);
                wchar_t b[128];
                swprintf(b, 128, L"Онлайн: %lld юзеров", j.at(L"users").i());
                ui.toast(b, false);
            } catch (...) { ui.toast(L"Ответ сервера", true); }
        }).detach();
    };
    ui.onOpenSite = [] {
        std::wstring u = g_cfg.server;
        ShellExecuteW(nullptr, L"open", u.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    };
    ui.onOpenCabinet = [] {
        std::wstring u = g_cfg.server;
        while (!u.empty() && u.back() == L'/') u.pop_back();
        ShellExecuteW(nullptr, L"open", (u + L"/cabinet.html").c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    };

    // initial data load
    std::thread([] {
        // server stats
        auto r = apiGet("/api/stats");
        if (r.code == 200) {
            try {
                auto j = json::parse(r.body);
                ui.serverOk = true;
                ui.serverUsers = j.at(L"users").i();
                ui.serverOnline = j.at(L"online").i();
            } catch (...) {}
        }
        auto b = apiGet("/api/builds");
        if (b.code == 200) {
            try {
                auto j = json::parse(b.body);
                if (j.at(L"builds").type == json::Value::ARR) ui.builds = j.at(L"builds");
            } catch (...) { ui.buildsErr = L"ответ сервера"; }
        } else ui.buildsErr = L"нет связи";
        // restore session
        if (!g_token.empty() || !g_cfg.token.empty()) {
            if (g_token.empty()) g_token = g_cfg.token;
            refreshMeNow();
        }
        // jar info line
        std::wstring mp = g_cfg.gameDir + L"\\mods\\etherial-client.jar";
        DWORD sz = 0;
        HANDLE f = CreateFileW(mp.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
        if (f != INVALID_HANDLE_VALUE) {
            sz = GetFileSize(f, nullptr);
            CloseHandle(f);
            if (sz > 1000000) {
                std::string h = crypto::fileSha256(mp);
                ui.jarInfo = std::to_wstring(sz / 1048576) + L" МБ · sha256 " +
                    http::narrow_to_wide(h.substr(0, 16)) + L"…";
            } else ui.jarInfo = L"файл битый — скачай заново";
        } else ui.jarInfo = L"не скачан";
        ui.invalidate();
    }).detach();

    if (!ui.create(h)) return 1;
    return ui.run();
}
