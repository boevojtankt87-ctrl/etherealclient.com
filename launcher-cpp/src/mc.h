// Full Minecraft bootstrap without the official launcher:
// Mojang manifests -> client+libraries+natives+assets, Fabric loader,
// Adoptium JRE, our mod from GitHub. Real downloads with SHA verify.
#pragma once
#include <string>
#include <vector>
#include <functional>
#include "json.h"
#include "http.h"
#include "crypto.h"

namespace mc {

const wchar_t* MC_VERSION = L"1.21.11";
const wchar_t* MOD_URL = L"https://raw.githubusercontent.com/boevojtankt87-ctrl/etherealclient.com/main/files/etherial-client.jar";
const wchar_t* FABRIC_META = L"https://meta.fabricmc.net";
const wchar_t* MOJANG_META = L"https://piston-meta.mojang.com";

inline std::wstring join(const std::wstring& a, const std::wstring& b) {
    if (!a.empty() && (a.back() == L'\\' || a.back() == L'/')) return a + b;
    return a + L"\\" + b;
}
inline std::wstring join(const std::wstring& a, const std::wstring& b, const std::wstring& c) {
    return join(join(a, b), c);
}

inline bool exists(const std::wstring& p) {
    return GetFileAttributesW(p.c_str()) != INVALID_FILE_ATTRIBUTES;
}
inline bool mkDirs(const std::wstring& p) {
    if (exists(p)) return true;
    size_t i = p.find_last_of(L"\\/");
    if (i != std::wstring::npos && !mkDirs(p.substr(0, i))) return false;
    return CreateDirectoryW(p.c_str(), nullptr) || GetLastError() == ERROR_ALREADY_EXISTS;
}
inline unsigned long long fsize(const std::wstring& p) {
    WIN32_FILE_ATTRIBUTE_DATA d{};
    if (!GetFileAttributesExW(p.c_str(), GetFileExInfoStandard, &d)) return 0;
    return ((unsigned long long)d.nFileSizeHigh << 32) | d.nFileSizeLow;
}
inline std::string w2u(const std::wstring& w) {
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string s(n - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &s[0], n, nullptr, nullptr);
    return s;
}

inline std::string w2s(const std::wstring& w) {
    if (w.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string s(n - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &s[0], n, nullptr, nullptr);
    return s;
}

struct Progress { std::wstring label; int pct = -1; }; // pct<0 idle

// Download url -> path, skip when sha1 matches (or size matches if no sha1).
inline bool fetch(const std::wstring& url, const std::wstring& path,
                  const std::string& sha1, Progress* pr, const std::wstring& label,
                  long long expected = -1) {
    if (exists(path) && !sha1.empty()) {
        if (crypto::fileSha1(path) == sha1) return true;
    } else if (exists(path) && sha1.empty() && fsize(path) > 1024) {
        return true;
    }
    size_t bs = path.find_last_of(L"\\/");
    if (bs != std::wstring::npos) mkDirs(path.substr(0, bs));
    if (!http::isLocal(url)) {
        if (pr) { pr->label = label; pr->pct = -2; }
        bool ok = http::curlDownload(url, path, expected,
            [&](long long d, long long t) { if (pr) pr->pct = t > 0 ? int(d * 100 / t) : -2; });
        if (pr) pr->pct = -1;
        if (!ok) return false;
        if (!sha1.empty() && crypto::fileSha1(path) != sha1) return false;
        return true;
    }
    std::vector<char> data;
    auto r = http::request(url, "", [&](long long d, long long t) {
        if (pr) { pr->label = label; pr->pct = t > 0 ? int(d * 100 / t) : -2; }
    }, &data);
    if (pr) pr->pct = -1;
    if (r.code != 200 || data.empty()) return false;
    if (!sha1.empty() && crypto::sha1hex(data.data(), data.size()) != sha1) return false;
    HANDLE f = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    DWORD wr = 0;
    WriteFile(f, data.data(), (DWORD)data.size(), &wr, nullptr);
    CloseHandle(f);
    return wr == data.size();
}

// Expand a zip/jar via OS powershell (no 3rd-party unzip needed).
inline bool expandZip(const std::wstring& zip, const std::wstring& dest) {
    mkDirs(dest);
    std::wstring cmd = L"powershell -NoProfile -NonInteractive -Command \"Expand-Archive -LiteralPath '"
        + zip + L"' -DestinationPath '" + dest + L"' -Force\"";
    STARTUPINFOW si{}; si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    std::vector<wchar_t> buf(cmd.begin(), cmd.end());
    buf.push_back(0);
    if (!CreateProcessW(nullptr, buf.data(), nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
        return false;
    WaitForSingleObject(pi.hProcess, 600000);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    return code == 0;
}

inline void findFiles(const std::wstring& dir, const std::wstring& name, std::vector<std::wstring>& out) {
    std::wstring q = join(dir, L"*");
    WIN32_FIND_DATAW fd{};
    HANDLE h = FindFirstFileW(q.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (!wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L"..")) continue;
        std::wstring p = join(dir, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) findFiles(p, name, out);
        else if (!_wcsicmp(fd.cFileName, name.c_str())) out.push_back(p);
    } while (FindNextFileW(h, &fd));
    FindNextFileW(h, nullptr);
    FindClose(h);
}

struct Lib { std::wstring path; }; // relative to gameDir/libraries

struct VerInfo {
    std::string clientUrl, clientSha1, assetUrl, assetSha1, assetId, logUrl, logPath;
    long long clientSize = 0;
    std::vector<std::tuple<std::string, std::string, std::string, long long>> libs; // url,sha1,rel,size
    std::vector<std::tuple<std::string, std::string, std::string, long long>> natives;
};

inline std::string mavenPath(const std::string& coord, const std::string& ext = ".jar") {
    // group:artifact:version[:classifier]
    std::vector<std::string> parts;
    size_t s = 0;
    for (;;) {
        size_t i = coord.find(':', s);
        parts.push_back(coord.substr(s, i == std::string::npos ? i : i - s));
        if (i == std::string::npos) break;
        s = i + 1;
    }
    std::string g = parts[0], a = parts[1], v = parts[2];
    std::string cls = parts.size() > 3 ? "-" + parts[3] : "";
    for (auto& c : g) if (c == '.') c = '/';
    return g + "/" + a + "/" + v + "/" + a + "-" + v + cls + ext;
}

inline bool osAllows(const json::Value& lib) {
    const json::Value& rules = lib.at(L"rules");
    if (rules.type != json::Value::ARR || rules.arr.empty()) return true;
    bool allowed = true;
    for (auto& r : rules.arr) {
        const json::Value& os = r.at(L"os");
        bool applies = true;
        if (os.type == json::Value::OBJ) {
            std::wstring name = os.at(L"name").s();
            if (!name.empty() && name != L"windows") applies = false;
        }
        if (applies) allowed = r.at(L"action").s() == L"allow";
    }
    return allowed;
}

inline bool loadVersion(VerInfo& out, std::wstring& err) {
    auto m = http::get(std::wstring(MOJANG_META) + L"/mc/game/version_manifest_v2.json");
    if (m.code != 200) { err = L"манифест версий"; return false; }
    json::Value root;
    try { root = json::parse(m.body); } catch (...) { err = L"манифест версий"; return false; }
    std::string vurl;
    for (auto& v : root.at(L"versions").arr) {
        if (v.at(L"id").s() == L"1.21.11") { vurl = w2s(v.at(L"url").s()); break; }
    }
    if (vurl.empty()) { err = L"1.21.11 нет в манифесте"; return false; }
    auto j = http::request(http::narrow_to_wide(vurl), "");
    if (j.code != 200) { err = L"json версии"; return false; }
    json::Value ver;
    try { ver = json::parse(j.body); } catch (...) { err = L"json версии"; return false; }
    {
        auto& d = ver.at(L"downloads").at(L"client");
        out.clientUrl = w2s(d.at(L"url").s());
        out.clientSha1 = w2s(d.at(L"sha1").s());
        out.clientSize = d.at(L"size").i(0);
    }
    {
        auto& a = ver.at(L"assetIndex");
        out.assetId = w2s(a.at(L"id").s());
        out.assetUrl = w2s(a.at(L"url").s());
        out.assetSha1 = w2s(a.at(L"sha1").s());
    }
    {
        auto& l = ver.at(L"logging").at(L"client").at(L"file");
        out.logUrl = w2s(l.at(L"url").s());
        out.logPath = "logging/log4j2.xml";
    }
    for (auto& lib : ver.at(L"libraries").arr) {
        if (!osAllows(lib)) continue;
        std::string nb = w2s(lib.at(L"name").s());
        auto& dl = lib.at(L"downloads");
        // natives?
        std::string kb;
        const json::Value& nat = lib.at(L"natives");
        if (nat.type == json::Value::OBJ) kb = w2s(nat.at(L"windows").s());
        if (!kb.empty()) {
            const json::Value& cls = dl.at(L"classifiers");
            bool found = false;
            for (auto& kv : cls.obj) {
                if (w2s(kv.first) == kb) {
                    std::string rel = "natives/" + mavenPath(nb);
                    out.natives.emplace_back(w2s(kv.second.at(L"url").s()),
                                             w2s(kv.second.at(L"sha1").s()), rel,
                                             kv.second.at(L"size").i(0));
                    found = true;
                    break;
                }
            }
            if (found) continue; // natives-only lib
        }
        const json::Value& art = dl.at(L"artifact");
        if (art.type != json::Value::OBJ) continue;
        out.libs.emplace_back(w2s(art.at(L"url").s()), w2s(art.at(L"sha1").s()),
                              std::string("libraries/") + w2s(art.at(L"path").s()),
                              art.at(L"size").i(0));
    }
    return true;
}

struct Game {
    std::wstring dir;          // gameDir (mc)
    std::vector<std::wstring> classpath; // absolute jars
    std::wstring nativesDir, assetsDir, logPath, assetId;

    bool prepare(Progress* pr, std::wstring& err, const std::wstring& modUrl) {
        mkDirs(dir);
        VerInfo v;
        if (!loadVersion(v, err)) return false;
        // client
        if (pr) { pr->label = L"клиент 1.21.11"; }
        if (!fetch(http::narrow_to_wide(v.clientUrl),
                   join(join(dir, L"versions"), L"1.21.11.jar"), v.clientSha1, pr, L"клиент 1.21.11",
                   v.clientSize)) {
            err = L"client jar"; return false;
        }
        classpath.push_back(join(join(dir, L"versions"), L"1.21.11.jar"));
        // libraries
        size_t i = 0;
        for (auto& [url, sha1, rel, size] : v.libs) {
            if (pr) { pr->label = L"библиотеки " + std::to_wstring(++i) + L"/" + std::to_wstring(v.libs.size()); }
            std::wstring p = dir + L"\\" + http::narrow_to_wide(rel);
            if (!fetch(http::narrow_to_wide(url), p, sha1, nullptr, L"", size)) { err = L"библиотека"; return false; }
            classpath.push_back(p);
        }
        // natives
        nativesDir = join(dir, L"natives");
        mkDirs(nativesDir);
        for (auto& [url, sha1, rel, size] : v.natives) {
            std::wstring p = dir + L"\\" + http::narrow_to_wide(rel);
            if (!fetch(http::narrow_to_wide(url), p, sha1, nullptr, L"", size)) { err = L"natives"; return false; }
            std::wstring tmp = join(nativesDir, L"_x");
            mkDirs(tmp);
            if (!expandZip(p, tmp)) { err = L"распаковка natives"; return false; }
            {
                std::wstring q = join(tmp, L"*");
                WIN32_FIND_DATAW fd{};
                HANDLE h = FindFirstFileW(q.c_str(), &fd);
                if (h != INVALID_HANDLE_VALUE) {
                    do {
                        std::wstring n = fd.cFileName;
                        if (n.size() > 4 && !wcscmp(n.c_str() + n.size() - 4, L".dll")) {
                            CopyFileW(join(tmp, n).c_str(), join(nativesDir, n).c_str(), FALSE);
                        }
                    } while (FindNextFileW(h, &fd));
                    FindClose(h);
                }
            }
        }
        // logging
        if (!v.logUrl.empty()) {
            std::wstring lp = dir + L"\\" + http::narrow_to_wide(v.logPath);
            fetch(http::narrow_to_wide(v.logUrl), lp, "", nullptr, L"");
            logPath = lp;
        }
        // assets
        assetsDir = join(dir, L"assets");
        assetId = http::narrow_to_wide(v.assetId);
        {
            std::wstring idx = join(join(assetsDir, L"indexes"), assetId + L".json");
            if (!fetch(http::narrow_to_wide(v.assetUrl), idx, v.assetSha1, nullptr, L"")) {
                err = L"asset index"; return false;
            }
            HANDLE f = CreateFileW(idx.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
            std::string js;
            if (f != INVALID_HANDLE_VALUE) {
                DWORD sz = GetFileSize(f, nullptr);
                js.resize(sz);
                DWORD rd = 0;
                ReadFile(f, js.data(), sz, &rd, nullptr);
                CloseHandle(f);
            }
            json::Value root;
            try { root = json::parse(js); } catch (...) { err = L"asset index"; return false; }
            size_t n = 0, total = root.at(L"objects").obj.size();
            for (auto& kv : root.at(L"objects").obj) {
                std::string h = w2s(kv.second.at(L"hash").s());
                long long asize = kv.second.at(L"size").i(0);
                std::wstring p = join(join(assetsDir, L"objects"), http::narrow_to_wide(h.substr(0, 2) + "/" + h));
                if (pr) { pr->label = L"ассеты " + std::to_wstring(++n) + L"/" + std::to_wstring(total); }
                std::wstring url = L"https://resources.download.minecraft.net/" + http::narrow_to_wide(h.substr(0, 2) + "/" + h);
                if (!fetch(url, p, h, nullptr, L"", asize)) { err = L"ассеты"; return false; }
            }
        }
        if (pr) pr->pct = -1;
        // fabric
        if (!ensureFabric(pr, err)) return false;
        // our mod from GitHub
        if (!modUrl.empty()) {
            if (pr) { pr->label = L"мод Etherial"; }
            std::wstring mods = join(dir, L"mods");
            mkDirs(mods);
            std::wstring mp = join(mods, L"etherial-client.jar");
            if (!fetch(modUrl, mp, "", nullptr, L"")) {
                // retry once (raw cdn hiccup); keep old file if present
                if (!exists(mp) || fsize(mp) < 1000000) { err = L"мод с GitHub"; return false; }
            }
            if (fsize(mp) < 1000000) { err = L"мод с GitHub"; return false; }
            classpath.push_back(mp);
        }
        return true;
    }

    bool ensureFabric(Progress* pr, std::wstring& err) {
        auto lv = http::get(std::wstring(FABRIC_META) + L"/v2/versions/loader/1.21.11");
        if (lv.code != 200) { err = L"fabric meta"; return false; }
        json::Value arr;
        try { arr = json::parse(lv.body); } catch (...) { err = L"fabric meta"; return false; }
        if (arr.type != json::Value::ARR || arr.arr.empty()) { err = L"fabric meta"; return false; }
        std::string lver = w2s(arr.arr[0].at(L"loader").at(L"version").s());
        auto pf = http::request(std::wstring(FABRIC_META) + L"/v2/versions/loader/1.21.11/"
            + http::narrow_to_wide(lver) + L"/profile/json", "");
        if (pf.code != 200) { err = L"fabric profile"; return false; }
        json::Value prof;
        try { prof = json::parse(pf.body); } catch (...) { err = L"fabric profile"; return false; }
        int i = 0;
        int total = (int)prof.at(L"libraries").arr.size();
        for (auto& lib : prof.at(L"libraries").arr) {
            std::string nb = w2s(lib.at(L"name").s());
            std::string ub = w2s(lib.at(L"url").s());
            if (ub.empty()) ub = "https://maven.fabricmc.net/";
            std::string rel = "fabric/" + mavenPath(nb);
            if (pr) { pr->label = L"fabric " + std::to_wstring(++i) + L"/" + std::to_wstring(total); }
            std::string durl = ub + rel.substr(7); // url already ends with /
            std::wstring p = dir + L"\\" + http::narrow_to_wide(rel);
            if (!fetch(http::narrow_to_wide(durl), p, "", nullptr, L"")) { err = L"fabric lib"; return false; }
            classpath.push_back(p);
        }
        if (pr) pr->pct = -1;
        return true;
    }

    bool ensureJava(std::wstring& javaw, Progress* pr, std::wstring& err) {
        std::vector<std::wstring> c;
        findFiles(join(dir, L"runtime"), L"javaw.exe", c);
        if (!c.empty()) { javaw = c[0]; return true; }
        if (pr) { pr->label = L"Java 21"; }
        std::wstring zip = join(dir, L"runtime\\jre.zip");
        if (!fetch(L"https://api.adoptium.net/v3/binary/latest/21/ga/windows/x64/jre/hotspot/normal/eclipse",
                   zip, "", pr, L"Java 21")) {
            err = L"скачивание Java"; return false;
        }
        if (!expandZip(zip, join(dir, L"runtime"))) { err = L"распаковка Java"; return false; }
        DeleteFileW(zip.c_str());
        c.clear();
        findFiles(join(dir, L"runtime"), L"javaw.exe", c);
        if (c.empty()) { err = L"javaw не найден"; return false; }
        javaw = c[0];
        if (pr) pr->pct = -1;
        return true;
    }

    bool launch(const std::wstring& javaw, const std::wstring& nick,
                const std::string& uuid, int ramGb, std::wstring& err) {
        std::wstring cp;
        for (auto& j : classpath) { cp += L"\"" + j + L"\";"; }
        std::wstring args = L"-Xmx" + std::to_wstring(ramGb) + L"G -Xms1G"
            L" -Djava.library.path=\"" + nativesDir + L"\""
            L" -Dminecraft.launcher.brand=etherial -Dminecraft.launcher.version=1.0";
        if (!logPath.empty()) args += L" -Dlog4j.configurationFile=\"" + logPath + L"\"";
        args += L" -cp " + cp + L" net.fabricmc.loader.impl.launch.knot.KnotClient"
            L" --username \"" + nick + L"\""
            L" --version 1.21.11 --gameDir \"" + dir + L"\""
            L" --assetsDir \"" + assetsDir + L"\" --assetIndex " + assetId
            + L" --uuid " + http::narrow_to_wide(uuid)
            + L" --accessToken 0 --userType legacy --versionType release";
        std::wstring cmd = L"\"" + javaw + L"\" " + args;
        STARTUPINFOW si{}; si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};
        std::vector<wchar_t> buf(cmd.begin(), cmd.end());
        buf.push_back(0);
        if (!CreateProcessW(nullptr, buf.data(), nullptr, nullptr, FALSE, 0,
                            nullptr, dir.c_str(), &si, &pi)) {
            err = L"запуск javaw (код " + std::to_wstring(GetLastError()) + L")";
            return false;
        }
        CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
        return true;
    }
};

} // namespace mc
