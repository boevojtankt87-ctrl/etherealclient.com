// Machine fingerprint + local config/token store.
#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <string>
#include <vector>
#include <algorithm>
#include <fstream>
#include "crypto.h"

namespace sysutil {

inline std::wstring appDir() {
    wchar_t* appdata = nullptr;
    // SHGetKnownFolderPath needs shell32; use env fallback (simple, no extra deps)
    DWORD n = GetEnvironmentVariableW(L"APPDATA", nullptr, 0);
    std::wstring base;
    if (n > 1) {
        base.resize(n - 1);
        GetEnvironmentVariableW(L"APPDATA", &base[0], n);
    } else {
        base = L".";
    }
    std::wstring d = base + L"\\Etherial";
    CreateDirectoryW(d.c_str(), nullptr);
    return d;
}

inline std::string machineId() {
    std::string parts;
    char cn[256] = {};
    DWORD cns = sizeof(cn);
    if (GetComputerNameA(cn, &cns)) parts += cn;
    parts += "|";
    char un[256] = {};
    DWORD uns = sizeof(un);
    if (GetUserNameA(un, &uns)) parts += un;
    ULONG sz = 0;
    GetAdaptersAddresses(AF_UNSPEC, 0, nullptr, nullptr, &sz);
    std::vector<unsigned char> buf(sz);
    std::vector<std::string> macs;
    if (GetAdaptersAddresses(AF_UNSPEC, 0, nullptr,
            (IP_ADAPTER_ADDRESSES*)buf.data(), &sz) == NO_ERROR) {
        for (auto* a = (IP_ADAPTER_ADDRESSES*)buf.data(); a; a = a->Next) {
            if (a->PhysicalAddressLength == 0) continue;
            if (a->OperStatus != IfOperStatusUp) continue;
            char m[32] = {};
            for (ULONG i = 0; i < a->PhysicalAddressLength && i < 8; i++)
                snprintf(m + strlen(m), sizeof(m) - strlen(m), "%02X", a->PhysicalAddress[i]);
            macs.push_back(m);
        }
    }
    std::sort(macs.begin(), macs.end());
    for (auto& m : macs) { parts += "|"; parts += m; }
    auto h = crypto::hash(CALG_SHA_256, parts.data(), parts.size());
    const char* abc = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
    std::string out;
    for (int i = 0; i < 12 && i < (int)h.size(); i++) {
        if (i > 0 && i % 4 == 0) out += '-';
        out += abc[h[i] % 32];
    }
    return out.size() == 14 ? out : "UNKNOWN-HWID";
}

struct Config {
    std::wstring server = L"http://cg839385.tw1.ru";
    std::wstring gameDir;   // .etherial/mc
    std::string token;
    int ramGb = 4;
    std::wstring gameCmd;   // custom launch override (rarely needed)

    static std::wstring path() { return appDir() + L"\\launcher.cfg"; }

    void load() {
        std::wstring p = path();
        HANDLE f = CreateFileW(p.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                               OPEN_EXISTING, 0, nullptr);
        if (f == INVALID_HANDLE_VALUE) {
            gameDir = appDir() + L"\\mc";
            return;
        }
        DWORD sz = GetFileSize(f, nullptr);
        std::string data(sz, 0);
        DWORD rd = 0;
        ReadFile(f, data.data(), sz, &rd, nullptr);
        CloseHandle(f);
        auto get = [&](const char* k) -> std::string {
            std::string key = std::string("\n") + k + "=";
            size_t i = data.find(key);
            if (i == std::string::npos) {
                if (data.rfind(k, 0) == 0) i = (size_t)-1; else return "";
            }
            size_t v = (i == (size_t)-1) ? strlen(k) + 1 : i + key.size();
            size_t e = data.find('\n', v);
            return data.substr(v, e == std::string::npos ? e : e - v);
        };
        std::string sv = get("server");
        if (!sv.empty()) { int n = MultiByteToWideChar(CP_UTF8, 0, sv.c_str(), -1, nullptr, 0); std::wstring w(n - 1, 0); MultiByteToWideChar(CP_UTF8, 0, sv.c_str(), -1, &w[0], n); server = w; }
        token = get("token");
        std::string gd = get("gamedir");
        gameDir = gd.empty() ? appDir() + L"\\mc" : http::narrow_to_wide(gd);
        try { ramGb = std::max(1, std::min(32, std::stoi(get("ram")))); } catch (...) {}
        std::string gc = get("gamecmd");
        if (!gc.empty()) gameCmd = http::narrow_to_wide(gc);
    }

    void save() const {
        auto w2u = [](const std::wstring& w) {
            int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
            std::string s(n - 1, 0);
            WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &s[0], n, nullptr, nullptr);
            return s;
        };
        std::string d = "server=" + w2u(server) + "\ntoken=" + token
            + "\ngamedir=" + w2u(gameDir) + "\nram=" + std::to_string(ramGb)
            + "\ngamecmd=" + w2u(gameCmd) + "\n";
        HANDLE f = CreateFileW(path().c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
        if (f == INVALID_HANDLE_VALUE) return;
        DWORD wr = 0;
        WriteFile(f, d.data(), (DWORD)d.size(), &wr, nullptr);
        CloseHandle(f);
    }
};

} // namespace sysutil
