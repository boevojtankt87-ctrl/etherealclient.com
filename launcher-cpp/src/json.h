// Minimal JSON: objects, arrays, strings (escapes incl \u), numbers, true/false/null.
#pragma once
#include <string>
#include <vector>
#include <map>
#include <stdexcept>
#include <cctype>
#include <cstdlib>

namespace json {

struct Value;
using Object = std::map<std::wstring, Value>;
using Array = std::vector<Value>;

struct Value {
    enum Type { NUL, BOOL, NUM, STR, ARR, OBJ } type = NUL;
    bool b = false;
    double num = 0;
    std::wstring str;
    Array arr;
    Object obj;
    static Value nul() { return Value(); }
    static Value boolean(bool v) { Value x; x.type = BOOL; x.b = v; return x; }
    static Value number(double v) { Value x; x.type = NUM; x.num = v; return x; }
    static Value string(const std::wstring& v) { Value x; x.type = STR; x.str = v; return x; }
    const Value& at(const std::wstring& k) const {
        static Value n;
        if (type != OBJ) return n;
        auto it = obj.find(k);
        return it == obj.end() ? n : it->second;
    }
    std::wstring s() const { return type == STR ? str : L""; }
    long long i(long long d = 0) const {
        if (type == NUM) return (long long)num;
        if (type == STR) { try { return std::stoll(str); } catch (...) {} }
        return d;
    }
    double d(double dd = 0) const { return type == NUM ? num : dd; }
};

class Parser {
public:
    explicit Parser(const std::string& s) : s_(s) {}
    Value run() {
        ws();
        Value v = value();
        ws();
        if (pos_ != s_.size()) fail("trailing");
        return v;
    }
private:
    const std::string& s_;
    size_t pos_ = 0;
    [[noreturn]] void fail(const char* m) { throw std::runtime_error(m); }
    void ws() { while (pos_ < s_.size() && isspace((unsigned char)s_[pos_])) pos_++; }
    Value value() {
        if (pos_ >= s_.size()) fail("eof");
        char c = s_[pos_];
        if (c == '{') return object();
        if (c == '[') return array();
        if (c == '"') return Value::string(string());
        if (s_.compare(pos_, 4, "true") == 0) { pos_ += 4; return Value::boolean(true); }
        if (s_.compare(pos_, 5, "false") == 0) { pos_ += 5; return Value::boolean(false); }
        if (s_.compare(pos_, 4, "null") == 0) { pos_ += 4; return Value::nul(); }
        return number();
    }
    Value object() {
        Value v; v.type = Value::OBJ;
        pos_++; ws();
        if (pos_ < s_.size() && s_[pos_] == '}') { pos_++; return v; }
        for (;;) {
            ws();
            if (pos_ >= s_.size() || s_[pos_] != '"') fail("key");
            std::wstring k = string();
            ws();
            if (pos_ >= s_.size() || s_[pos_] != ':') fail("colon");
            pos_++;
            ws();
            v.obj[k] = value();
            ws();
            if (pos_ >= s_.size()) fail("eof obj");
            char c = s_[pos_++];
            if (c == '}') return v;
            if (c != ',') fail("comma");
        }
    }
    Value array() {
        Value v; v.type = Value::ARR;
        pos_++; ws();
        if (pos_ < s_.size() && s_[pos_] == ']') { pos_++; return v; }
        for (;;) {
            ws();
            v.arr.push_back(value());
            ws();
            if (pos_ >= s_.size()) fail("eof arr");
            char c = s_[pos_++];
            if (c == ']') return v;
            if (c != ',') fail("comma");
        }
    }
    std::wstring string() {
        std::wstring out;
        pos_++; // "
        while (true) {
            if (pos_ >= s_.size()) fail("eof str");
            char c = s_[pos_++];
            if (c == '"') return out;
            if (c == '\\') {
                if (pos_ >= s_.size()) fail("eof esc");
                char e = s_[pos_++];
                switch (e) {
                    case '"': out += L'"'; break;
                    case '\\': out += L'\\'; break;
                    case '/': out += L'/'; break;
                    case 'b': out += L'\b'; break;
                    case 'f': out += L'\f'; break;
                    case 'n': out += L'\n'; break;
                    case 'r': out += L'\r'; break;
                    case 't': out += L'\t'; break;
                    case 'u': {
                        if (pos_ + 4 > s_.size()) fail("bad u");
                        unsigned cp = 0;
                        for (int i = 0; i < 4; i++) {
                            char h = s_[pos_++];
                            cp <<= 4;
                            if (h >= '0' && h <= '9') cp += h - '0';
                            else if (h >= 'a' && h <= 'f') cp += h - 'a' + 10;
                            else if (h >= 'A' && h <= 'F') cp += h - 'A' + 10;
                            else fail("bad u");
                        }
                        if (cp >= 0xD800 && cp <= 0xDBFF && pos_ + 6 <= s_.size()
                            && s_[pos_] == '\\' && s_[pos_ + 1] == 'u') {
                            pos_ += 2;
                            unsigned lo = 0;
                            for (int i = 0; i < 4; i++) {
                                char h = s_[pos_++];
                                lo <<= 4;
                                if (h >= '0' && h <= '9') lo += h - '0';
                                else if (h >= 'a' && h <= 'f') lo += h - 'a' + 10;
                                else if (h >= 'A' && h <= 'F') lo += h - 'A' + 10;
                                else fail("bad u");
                            }
                            cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                        }
                        if (cp > 0xFFFF) { cp -= 0x10000; out += (wchar_t)(0xD800 + (cp >> 10)); out += (wchar_t)(0xDC00 + (cp & 0x3FF)); }
                        else out += (wchar_t)cp;
                        break;
                    }
                    default: fail("bad esc");
                }
            } else {
                // UTF-8 decode
                unsigned char uc = (unsigned char)c;
                if (uc < 0x80) out += (wchar_t)uc;
                else {
                    unsigned cp; int extra;
                    if ((uc & 0xE0) == 0xC0) { cp = uc & 0x1F; extra = 1; }
                    else if ((uc & 0xF0) == 0xE0) { cp = uc & 0x0F; extra = 2; }
                    else if ((uc & 0xF8) == 0xF0) { cp = uc & 0x07; extra = 3; }
                    else fail("bad utf8");
                    for (int i = 0; i < extra; i++) {
                        if (pos_ >= s_.size()) fail("eof utf8");
                        unsigned char cc = (unsigned char)s_[pos_++];
                        if ((cc & 0xC0) != 0x80) fail("bad utf8");
                        cp = (cp << 6) | (cc & 0x3F);
                    }
                    if (cp > 0xFFFF) { cp -= 0x10000; out += (wchar_t)(0xD800 + (cp >> 10)); out += (wchar_t)(0xDC00 + (cp & 0x3FF)); }
                    else out += (wchar_t)cp;
                }
            }
        }
    }
    Value number() {
        size_t st = pos_;
        while (pos_ < s_.size() && strchr("-+0123456789.eE", s_[pos_])) pos_++;
        std::string t = s_.substr(st, pos_ - st);
        if (t.empty()) fail("bad num");
        return Value::number(strtod(t.c_str(), nullptr));
    }
};

inline Value parse(const std::string& s) { return Parser(s).run(); }

} // namespace json
