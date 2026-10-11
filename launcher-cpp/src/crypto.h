// Hashing via CryptoAPI: SHA-1 (mojang files), SHA-256 (hwid), MD5 (offline uuid).
#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>
#include <string>
#include <vector>

namespace crypto {

inline std::vector<unsigned char> hash(ALG_ID alg, const void* data, size_t len) {
    std::vector<unsigned char> out;
    HCRYPTPROV prov = 0;
    if (!CryptAcquireContextW(&prov, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT))
        return out;
    HCRYPTHASH h = 0;
    if (CryptCreateHash(prov, alg, 0, 0, &h)) {
        if (CryptHashData(h, (const BYTE*)data, (DWORD)len, 0)) {
            DWORD sz = 0, sl = sizeof(sz);
            if (CryptGetHashParam(h, HP_HASHSIZE, (BYTE*)&sz, &sl, 0)) {
                out.resize(sz);
                DWORD rl = sz;
                if (!CryptGetHashParam(h, HP_HASHVAL, out.data(), &rl, 0)) out.clear();
            }
        }
        CryptDestroyHash(h);
    }
    CryptReleaseContext(prov, 0);
    return out;
}

inline std::vector<unsigned char> hashFile(ALG_ID alg, const std::wstring& path) {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    std::vector<unsigned char> empty;
    if (f == INVALID_HANDLE_VALUE) return empty;
    HCRYPTPROV prov = 0;
    std::vector<unsigned char> out;
    if (CryptAcquireContextW(&prov, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
        HCRYPTHASH h = 0;
        if (CryptCreateHash(prov, alg, 0, 0, &h)) {
            char buf[65536];
            DWORD rd = 0;
            BOOL ok = TRUE;
            while (ok && ReadFile(f, buf, sizeof(buf), &rd, nullptr) && rd > 0)
                ok = CryptHashData(h, (const BYTE*)buf, rd, 0);
            if (ok) {
                DWORD sz = 0, sl = sizeof(sz);
                if (CryptGetHashParam(h, HP_HASHSIZE, (BYTE*)&sz, &sl, 0)) {
                    out.resize(sz);
                    DWORD rl = sz;
                    if (!CryptGetHashParam(h, HP_HASHVAL, out.data(), &rl, 0)) out.clear();
                }
            }
            CryptDestroyHash(h);
        }
        CryptReleaseContext(prov, 0);
    }
    CloseHandle(f);
    return out;
}

inline std::string hex(const std::vector<unsigned char>& v) {
    static const char* H = "0123456789abcdef";
    std::string s;
    s.reserve(v.size() * 2);
    for (auto b : v) { s += H[b >> 4]; s += H[b & 15]; }
    return s;
}

inline std::string sha1hex(const void* d, size_t n) { return hex(hash(CALG_SHA1, d, n)); }
inline std::string sha1hex(const std::string& s) { return sha1hex(s.data(), s.size()); }
inline std::string sha256hex(const void* d, size_t n) { return hex(hash(CALG_SHA_256, d, n)); }

inline std::string fileSha1(const std::wstring& path) { return hex(hashFile(CALG_SHA1, path)); }
inline std::string fileSha256(const std::wstring& path) { return hex(hashFile(CALG_SHA_256, path)); }

// Offline-mode UUID: MD5("OfflinePlayer:"+nick), version 3, formatted 8-4-4-4-12.
inline std::string offlineUuid(const std::string& nick) {
    std::string src = "OfflinePlayer:" + nick;
    auto h = hash(CALG_MD5, src.data(), src.size());
    if (h.size() != 16) return "00000000-0000-0000-0000-000000000000";
    h[6] = (h[6] & 0x0F) | 0x30;
    h[8] = (h[8] & 0x3F) | 0x80;
    char b[37];
    snprintf(b, sizeof(b), "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             h[0], h[1], h[2], h[3], h[4], h[5], h[6], h[7],
             h[8], h[9], h[10], h[11], h[12], h[13], h[14], h[15]);
    return std::string(b);
}

} // namespace crypto
