// Sync HTTP: WinINet for localhost API (proven), curl.exe for everything
// external (robust TLS/redirects, stock on Windows 10+).
#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wininet.h>
#include <string>
#include <vector>
#include <functional>

namespace http {

inline std::wstring narrow_to_wide(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
    return w;
}
inline std::string wide_to_narrow(const std::wstring& w) {
    if (w.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string s(n - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &s[0], n, nullptr, nullptr);
    return s;
}

struct Result {
    long code = 0;
    std::string body;
    std::wstring error;
};

inline bool isLocal(const std::wstring& url) {
    std::wstring u = url;
    for (auto& c : u) c = towlower(c);
    return u.find(L"://localhost") != std::wstring::npos
        || u.find(L"://127.0.0.1") != std::wstring::npos;
}

// curl.exe download with real progress (size polling). Returns http code.
// curl.exe download with real progress (size polling).
inline bool curlDownload(const std::wstring& url, const std::wstring& outPath,
                         long long expectedTotal,
                         std::function<void(long long, long long)> progress) {
    std::wstring cmd = L"curl.exe -sS -L --fail --max-time 1200 -o \"" + outPath + L"\" \"" + url + L"\"";
    std::vector<wchar_t> cb(cmd.begin(), cmd.end());
    cb.push_back(0);
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(nullptr, cb.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                        nullptr, nullptr, &si, &pi))
        return false;
    bool ok = false;
    for (;;) {
        DWORD w = WaitForSingleObject(pi.hProcess, 120);
        if (progress) {
            WIN32_FILE_ATTRIBUTE_DATA fd{};
            long long sz = 0;
            if (GetFileAttributesExW(outPath.c_str(), GetFileExInfoStandard, &fd))
                sz = (long long)(((unsigned long long)fd.nFileSizeHigh << 32) | fd.nFileSizeLow);
            progress(sz, expectedTotal);
        }
        if (w != WAIT_TIMEOUT) {
            DWORD ec = 1;
            GetExitCodeProcess(pi.hProcess, &ec);
            ok = (ec == 0);
            break;
        }
    }
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    if (progress) progress(expectedTotal > 0 ? expectedTotal : 0, expectedTotal);
    return ok;
}

// Small text GET over curl (JSON and alike). Returns http code, -1 on error.
inline long curlText(const std::wstring& url, std::string& out) {
    std::wstring cmd = L"curl.exe -sS -L --fail --max-time 60 \"" + url + L"\"";
    std::vector<wchar_t> cb(cmd.begin(), cmd.end());
    cb.push_back(0);
    SECURITY_ATTRIBUTES sa{ sizeof(sa), nullptr, TRUE };
    HANDLE rd = nullptr, wr = nullptr;
    if (!CreatePipe(&rd, &wr, &sa, 0)) return -1;
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = wr; si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION pi{};
    long code = -1;
    if (CreateProcessW(nullptr, cb.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
                       nullptr, nullptr, &si, &pi)) {
        CloseHandle(wr);
        char buf[32768];
        DWORD got = 0;
        std::string all;
        for (;;) {
            for (;;) {
                DWORD avail = 0;
                if (!PeekNamedPipe(rd, nullptr, 0, nullptr, &avail, nullptr) || avail == 0) break;
                if (!ReadFile(rd, buf, sizeof(buf), &got, nullptr) || got == 0) break;
                all.append(buf, got);
            }
            if (WaitForSingleObject(pi.hProcess, 120) != WAIT_TIMEOUT) break;
        }
        // final drain
        for (;;) {
            DWORD avail = 0;
            if (!PeekNamedPipe(rd, nullptr, 0, nullptr, &avail, nullptr) || avail == 0) break;
            if (!ReadFile(rd, buf, sizeof(buf), &got, nullptr) || got == 0) break;
            all.append(buf, got);
        }
        DWORD ec = 1;
        GetExitCodeProcess(pi.hProcess, &ec);
        CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
        if (ec == 0 && !all.empty()) { out = all; code = 200; }
    }
    CloseHandle(rd);
    return code;
}

inline Result request(const std::wstring& url, const std::string& post,
                      std::function<void(long long, long long)> progress = nullptr,
                      std::vector<char>* outBin = nullptr) {
    Result r;
    // WinINet for every text request (localhost and external API alike);
    // curl is only used for large file downloads (curlDownload).
    HINTERNET inet = InternetOpenW(L"EtherialLauncher/1.0", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
    if (!inet) { r.error = L"no inet"; return r; }
    std::wstring u = url;
    for (int redir = 0; redir < 6; redir++) {
        URL_COMPONENTSW parts = { sizeof(parts) };
        wchar_t host[256] = {}, path[4096] = {};
        parts.lpszHostName = host; parts.dwHostNameLength = 256;
        parts.lpszUrlPath = path; parts.dwUrlPathLength = 4096;
        parts.dwSchemeLength = 32;
        if (!InternetCrackUrlW(u.c_str(), (DWORD)u.size(), 0, &parts)) { r.error = L"bad url"; break; }
        std::wstring fullPath = parts.lpszUrlPath;
        HINTERNET conn = InternetConnectW(inet, host, parts.nPort, nullptr, nullptr,
                                          INTERNET_SERVICE_HTTP, 0, 0);
        if (!conn) { r.error = L"connect"; break; }
        DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE
                    | INTERNET_FLAG_KEEP_CONNECTION;
        if (parts.nScheme == INTERNET_SCHEME_HTTPS) flags |= INTERNET_FLAG_SECURE;
        // GET when post empty; POST otherwise.
        bool isPost = !post.empty();
        HINTERNET req = HttpOpenRequestW(conn, isPost ? L"POST" : L"GET", fullPath.c_str(),
                                         L"HTTP/1.1", nullptr, nullptr, flags, 0);
        if (!req) { InternetCloseHandle(conn); r.error = L"open req"; break; }
        std::wstring hdrs = L"Content-Type: application/json\r\nAccept: */*\r\n";
        BOOL sent = HttpSendRequestW(req, hdrs.c_str(), (DWORD)hdrs.size(),
                                     isPost ? (LPVOID)post.data() : nullptr,
                                     isPost ? (DWORD)post.size() : 0);
        if (!sent) {
            DWORD err = GetLastError();
            InternetCloseHandle(req); InternetCloseHandle(conn);
            if (err == ERROR_INTERNET_CONNECTION_RESET && redir == 0) continue;
            r.error = L"send";
            break;
        }
        DWORD status = 0, slen = sizeof(status);
        HttpQueryInfoW(req, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &status, &slen, nullptr);
        r.code = (long)status;
        if (status == 301 || status == 302 || status == 303 || status == 307 || status == 308) {
            wchar_t loc[4096] = {};
            DWORD llen = 4096;
            if (HttpQueryInfoW(req, HTTP_QUERY_LOCATION, loc, &llen, nullptr)) {
                std::wstring nu = loc;
                if (nu.find(L"://") == std::wstring::npos && !nu.empty() && nu[0] == L'/') {
                    wchar_t scheme[32] = {};
                    URL_COMPONENTSW p2 = { sizeof(p2) };
                    p2.lpszHostName = host; p2.dwHostNameLength = 256;
                    p2.dwSchemeLength = 32; p2.lpszScheme = scheme;
                    InternetCrackUrlW(u.c_str(), (DWORD)u.size(), 0, &p2);
                    std::wstring port = (parts.nPort == 80 || parts.nPort == 443) ? L"" : (L":" + std::to_wstring(parts.nPort));
                    nu = std::wstring(scheme) + L"://" + host + port + nu;
                }
                InternetCloseHandle(req); InternetCloseHandle(conn);
                u = nu;
                // 303 always becomes GET; keep body only for 307/308
                continue;
            }
        }
        // read body
        DWORD avail = 0;
        InternetQueryDataAvailable(req, &avail, 0, 0);
        DWORD total = 0;
        {
            wchar_t cl[64] = {};
            DWORD cllen = 64;
            if (HttpQueryInfoW(req, HTTP_QUERY_CONTENT_LENGTH, cl, &cllen, nullptr))
                total = (DWORD)_wtol(cl);
        }
        std::string body;
        char buf[65536];
        DWORD got = 0;
        long long done = 0;
        for (;;) {
            DWORD rd = 0;
            if (!InternetReadFile(req, buf, sizeof(buf), &rd) || rd == 0) break;
            body.append(buf, rd);
            done += rd;
            if (progress && total > 0) progress(done, total);
            (void)got;
        }
        if (outBin) {
            outBin->assign(body.begin(), body.end());
            if (progress) progress(done, done);
        } else {
            r.body = body;
        }
        InternetCloseHandle(req);
        InternetCloseHandle(conn);
        break;
    }
    InternetCloseHandle(inet);
    return r;
}

inline Result get(const std::wstring& url) { return request(url, ""); }
inline Result post(const std::wstring& url, const std::string& json) { return request(url, json); }

} // namespace http
