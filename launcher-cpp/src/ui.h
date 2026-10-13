// Borderless custom-rendered window: titlebar, buttons, fields, slider, lists.
#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <string>
#include <vector>
#include <list>
#include <functional>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <objidl.h>
#include <gdiplus.h>
#include "json.h"

namespace ui {

const int WIN_W = 920, WIN_H = 640;
const int TITLE_H = 40;
const COLORREF C_BG = RGB(10, 12, 11), C_PANEL = RGB(16, 20, 19), C_PANEL2 = RGB(21, 27, 25);
const COLORREF C_LINE = RGB(32, 40, 38), C_TXT = RGB(242, 245, 243);
const COLORREF C_MUT = RGB(154, 169, 162), C_DIM = RGB(98, 113, 106);
const COLORREF C_ACC = RGB(78, 186, 142), C_ACC_DK = RGB(4, 17, 12);
const COLORREF C_RED = RGB(255, 81, 71), C_GOLD = RGB(216, 179, 74);

struct Button {
    RECT rc{}; std::wstring text; std::function<void()> act;
    bool solid = false, link = false, enabled = true;
    float hov = 0; // animated 0..1
    bool hit(int x, int y) const { return enabled && x >= rc.left && x <= rc.right && y >= rc.top && y <= rc.bottom; }
};

    struct Field {
        std::wstring hint, icon;
        std::wstring text;
        size_t max = 64;
        bool pass = false;
        int x = 0, y = 0, w = 0, h = 0;
        Field() = default;
        Field(const std::wstring& h, const std::wstring& i, size_t m, bool p)
            : hint(h), icon(i), max(m), pass(p) {}
        bool hit(int x, int y) const { return x >= this->x && x <= this->x + w && y >= this->y && y <= this->y + h; }
        std::wstring value() const { return text; }
        void set(const std::wstring& s) { text = s; }
    };

struct Slider {
    RECT rc{}; int min = 1, max = 16, val = 4;
    bool drag = false;
    bool hit(int x, int y) const {
        return x >= rc.left - 10 && x <= rc.right + 10 && y >= rc.top - 14 && y <= rc.bottom + 14;
    }
    void fromX(int x) {
        double k = double(x - rc.left) / double(std::max(1L, rc.right - rc.left));
        val = std::min(max, std::max(min, int(min + k * (max - min) + 0.5)));
    }
};

struct Toast { std::wstring text; bool err; DWORD until; };

class Ui {
public:
    enum class Screen { LOGIN, MAIN };
    enum class Tab { PLAY, NEWS, PROFILE, SET };

    // shared state (written by worker threads, read on paint)
    Screen screen = Screen::LOGIN;
    Tab tab = Tab::PLAY;
    bool forgotMode = false;
    json::Value me; bool hasMe = false;
    json::Value builds = json::Value::nul(); // array
    std::wstring buildsErr;
    bool serverOk = false;
    long long serverUsers = 0, serverOnline = 0;
    std::wstring jarInfo = L"проверка…";
    std::wstring statusLine;
    int dlProgress = -1;
    std::wstring lastLaunched;
    int ramGb = 4;

    // actions wired by main.cpp
    std::function<void()> onLogin, onRecover, onLaunch, onDownload, onLogout,
        onSaveSettings, onTestConn, onOpenSite, onOpenCabinet;

    Field nickF{L"Username", L"user", 16, false}, passF{L"Password", L"lock", 32, true};
    Field recNickF{L"Username", L"user", 16, false}, recMailF{L"Email", L"mail", 64, false},
          recPassF{L"Новый пароль", L"lock", 32, true};
    Field serverF, cmdF;
    Field* focused = nullptr;

    std::vector<Button*> buttons;
    Slider ramSlider;
    int mx = -1, my = -1, scrollY = 0;
    std::vector<Toast> toasts;
    Gdiplus::Bitmap* logoBmp_ = nullptr;
    ULONG_PTR gdiToken_ = 0;
    bool verOpen = false;
    std::vector<std::wstring> versions = { L"1.21.11" };
    int verSel = 0;

    Ui() {
        serverF.hint = L"Адрес сервера"; serverF.max = 64;
        cmdF.hint = L"Команда запуска игры"; cmdF.max = 256;
    }

    void toast(const std::wstring& t, bool err = false) {
        toasts.push_back({t, err, GetTickCount() + 3400});
    }

    bool create(HINSTANCE h) {
        hInst_ = h;
        WNDCLASSW wc{};
        wc.lpfnWndProc = &Ui::procThunk;
        wc.hInstance = h;
        wc.lpszClassName = L"EtherealLauncher";
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        HICON ico = (HICON)LoadImageW(h, L"IDI_ICON1", IMAGE_ICON, 0, 0, LR_DEFAULTSIZE);
        // icon comes from resources of the exe; fallback none
        wc.hIcon = ico;
        RegisterClassW(&wc);
        hwnd_ = CreateWindowExW(WS_EX_APPWINDOW, L"EtherealLauncher", L"Ethereal Launcher",
            WS_POPUP | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, WIN_W, WIN_H,
            nullptr, nullptr, h, this);
        if (!hwnd_) return false;
        // center
        RECT r; GetWindowRect(hwnd_, &r);
        int sx = GetSystemMetrics(SM_CXSCREEN), sy = GetSystemMetrics(SM_CYSCREEN);
        SetWindowPos(hwnd_, nullptr, (sx - WIN_W) / 2, (sy - WIN_H) / 2, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        // GDI+ for the embedded logo
        Gdiplus::GdiplusStartupInput gsi;
        if (Gdiplus::GdiplusStartup(&gdiToken_, &gsi, nullptr) == Gdiplus::Ok) {
            HRSRC hr = FindResourceW(hInst_, L"LOGO_JPG", RT_RCDATA);
            if (hr) {
                DWORD sz = SizeofResource(hInst_, hr);
                void* pd = LockResource(LoadResource(hInst_, hr));
                if (pd && sz) {
                    HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, sz);
                    if (hg) {
                        memcpy(GlobalLock(hg), pd, sz);
                        GlobalUnlock(hg);
                        IStream* st = nullptr;
                        if (CreateStreamOnHGlobal(hg, TRUE, &st) == S_OK) {
                            Gdiplus::Bitmap* bmp = Gdiplus::Bitmap::FromStream(st);
                            if (bmp && bmp->GetLastStatus() == Gdiplus::Ok)
                                logoBmp_ = bmp;
                            else delete bmp;
                            st->Release();
                        } else GlobalFree(hg);
                    }
                }
            }
        }
        SetTimer(hwnd_, 1, 16, nullptr);
        return true;
    }

    int run() {
        MSG m;
        while (GetMessageW(&m, nullptr, 0, 0)) { TranslateMessage(&m); DispatchMessageW(&m); }
        return (int)m.wParam;
    }

    void invalidate() { if (hwnd_) InvalidateRect(hwnd_, nullptr, FALSE); }
    void notifyData() { if (hwnd_) PostMessageW(hwnd_, WM_APP + 1, 0, 0); }

    HFONT font(int px, bool bold) {
        // tiny cache
        for (auto& f : fonts_) if (f.px == px && f.bold == bold) return f.h;
        HFONT h = CreateFontW(px, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH, L"Segoe UI");
        fonts_.push_back({px, bold, h});
        return h;
    }

private:
    struct FontE { int px; bool bold; HFONT h; };
    HWND hwnd_ = nullptr;
    HINSTANCE hInst_ = nullptr;
    std::vector<FontE> fonts_;
    DWORD tick0_ = GetTickCount();

    static LRESULT CALLBACK procThunk(HWND h, UINT m, WPARAM w, LPARAM l) {
        Ui* self = nullptr;
        if (m == WM_NCCREATE) {
            self = (Ui*)((CREATESTRUCTW*)l)->lpCreateParams;
            SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)self);
            self->hwnd_ = h;
        } else self = (Ui*)GetWindowLongPtrW(h, GWLP_USERDATA);
        return self ? self->proc(m, w, l) : DefWindowProcW(h, m, w, l);
    }

    // ----- helpers -----
    static void rr(HDC dc, int x, int y, int w, int h, int r, COLORREF fill, COLORREF line, bool doLine = true) {
        HBRUSH b = CreateSolidBrush(fill);
        HPEN p = CreatePen(PS_SOLID, 1, line);
        HGDIOBJ ob = SelectObject(dc, b), op = SelectObject(dc, p);
        RoundRect(dc, x, y, x + w, y + h, r * 2, r * 2);
        if (!doLine) { SelectObject(dc, GetStockObject(NULL_PEN)); RoundRect(dc, x, y, x + w, y + h, r * 2, r * 2); }
        SelectObject(dc, ob); SelectObject(dc, op);
        DeleteObject(b); DeleteObject(p);
    }
    void txt(HDC dc, int px, bool bold, COLORREF c, const std::wstring& s, int x, int y) {
        HFONT f = font(px, bold);
        HGDIOBJ o = SelectObject(dc, f);
        SetTextColor(dc, c);
        SetBkMode(dc, TRANSPARENT);
        TextOutW(dc, x, y, s.c_str(), (int)s.size());
        SelectObject(dc, o);
    }
    int tw(HDC dc, int px, bool bold, const std::wstring& s) {
        HGDIOBJ o = SelectObject(dc, font(px, bold));
        SIZE z{}; GetTextExtentPoint32W(dc, s.c_str(), (int)s.size(), &z);
        SelectObject(dc, o);
        return z.cx;
    }
    void icon(HDC dc, const std::wstring& kind, int x, int y, int s, COLORREF c) {
        HPEN pen = CreatePen(PS_SOLID, std::max(1, s / 10), c);
        HGDIOBJ op = SelectObject(dc, pen);
        HBRUSH br = CreateSolidBrush(c);
        HGDIOBJ obr = SelectObject(dc, br);
        int cx = x + s / 2, cy = y + s / 2, u = std::max(1, s / 12);
        if (kind == L"user") {
            Ellipse(dc, cx - 3 * u, cy - 5 * u, cx + 3 * u, cy + u);
            RoundRect(dc, cx - 6 * u, cy + 2 * u, cx + 6 * u, cy + 8 * u, 3 * u, 3 * u);
        } else if (kind == L"lock") {
            RoundRect(dc, cx - 5 * u, cy - u, cx + 5 * u, cy + 7 * u, 2 * u, 2 * u);
            Arc(dc, cx - 3 * u, cy - 6 * u, cx + 3 * u, cy, 0, 0, 0, 0);
            Ellipse(dc, cx - u, cy + u, cx + u, cy + 4 * u);
        } else if (kind == L"mail") {
            RoundRect(dc, x + u, cy - 4 * u, x + 11 * u, cy + 4 * u, 2 * u, 2 * u);
            MoveToEx(dc, x + u, cy - 4 * u, nullptr); LineTo(dc, cx, cy + u);
            MoveToEx(dc, x + 11 * u, cy - 4 * u, nullptr); LineTo(dc, cx, cy + u);
        } else if (kind == L"ram") {
            RoundRect(dc, cx - 6 * u, cy - 4 * u, cx + 6 * u, cy + 4 * u, 2 * u, 2 * u);
            MoveToEx(dc, cx - 2 * u, cy - 4 * u, nullptr); LineTo(dc, cx - 2 * u, cy - 6 * u);
            MoveToEx(dc, cx + 2 * u, cy - 4 * u, nullptr); LineTo(dc, cx + 2 * u, cy - 6 * u);
            RECT r{ cx - 3 * u, cy - u, cx + 3 * u, cy + u };
            FillRect(dc, &r, br);
        } else if (kind == L"play") {
            POINT p[3] = { {cx - 3 * u, cy - 5 * u}, {cx - 3 * u, cy + 5 * u}, {cx + 5 * u, cy} };
            Polygon(dc, p, 3);
        } else if (kind == L"dl") {
            MoveToEx(dc, cx, cy - 6 * u, nullptr); LineTo(dc, cx, cy + 3 * u);
            MoveToEx(dc, cx - 4 * u, cy - u, nullptr); LineTo(dc, cx, cy + 3 * u);
            MoveToEx(dc, cx + 4 * u, cy - u, nullptr); LineTo(dc, cx, cy + 3 * u);
            MoveToEx(dc, cx - 6 * u, cy + 5 * u, nullptr); LineTo(dc, cx + 6 * u, cy + 5 * u);
        } else if (kind == L"news") {
            MoveToEx(dc, x + 2 * u, cy - 3 * u, nullptr); LineTo(dc, x + 10 * u, cy - 3 * u);
            MoveToEx(dc, x + 2 * u, cy, nullptr); LineTo(dc, x + 10 * u, cy);
            MoveToEx(dc, x + 2 * u, cy + 3 * u, nullptr); LineTo(dc, x + 7 * u, cy + 3 * u);
        } else if (kind == L"gear") {
            Ellipse(dc, cx - 4 * u, cy - 4 * u, cx + 4 * u, cy + 4 * u);
            for (int a = 0; a < 8; a++) {
                double r = 3.14159 * a / 4;
                MoveToEx(dc, int(cx + cos(r) * 5 * u), int(cy + sin(r) * 5 * u), nullptr);
                LineTo(dc, int(cx + cos(r) * 7 * u), int(cy + sin(r) * 7 * u));
            }
        } else if (kind == L"home") {
            MoveToEx(dc, cx - 6 * u, cy, nullptr); LineTo(dc, cx, cy - 6 * u); LineTo(dc, cx + 6 * u, cy);
            RoundRect(dc, cx - 4 * u, cy - 1 * u, cx + 4 * u, cy + 6 * u, 2 * u, 2 * u);
        } else if (kind == L"out") {
            RoundRect(dc, x + u, cy - 5 * u, x + 8 * u, cy + 5 * u, 2 * u, 2 * u);
            MoveToEx(dc, cx - 2 * u, cy, nullptr); LineTo(dc, cx + 4 * u, cy);
            MoveToEx(dc, cx + 1 * u, cy - 3 * u, nullptr); LineTo(dc, cx + 4 * u, cy);
            MoveToEx(dc, cx + 1 * u, cy + 3 * u, nullptr); LineTo(dc, cx + 4 * u, cy);
        } else {
            Ellipse(dc, cx - 2 * u, cy - 2 * u, cx + 2 * u, cy + 2 * u);
        }
        SelectObject(dc, op); SelectObject(dc, obr);
        DeleteObject(pen); DeleteObject(br);
    }

    void drawButton(HDC dc, Button& b) {
        buttons.push_back(&b);
        bool hov = mx >= b.rc.left && mx <= b.rc.right && my >= b.rc.top && my <= b.rc.bottom;
        b.hov += ((hov ? 1.f : 0.f) - b.hov) * 0.35f;
        if (b.link) {
            txt(dc, 14, false, b.hov > 0.5f ? C_ACC : C_MUT, b.text,
                b.rc.left + (b.rc.right - b.rc.left - tw(dc, 14, false, b.text)) / 2, b.rc.top);
            return;
        }
        COLORREF fill = !b.enabled ? RGB(42, 50, 48)
            : b.solid ? RGB(78 + int(30 * b.hov), 186, 142) : (b.hov > 0.5f ? C_PANEL2 : C_PANEL);
        COLORREF ln = !b.enabled ? C_LINE : (b.solid ? fill : (b.hov > 0.5f ? C_ACC : C_LINE));
        rr(dc, b.rc.left, b.rc.top, b.rc.right - b.rc.left, b.rc.bottom - b.rc.top, 12, fill, ln);
        txt(dc, 14, false, !b.enabled ? C_DIM : (b.solid ? C_ACC_DK : C_TXT), b.text,
            b.rc.left + (b.rc.right - b.rc.left - tw(dc, 14, false, b.text)) / 2,
            b.rc.top + (b.rc.bottom - b.rc.top - 18) / 2);
    }
    Button* mkBtn(int x, int y, int w, int h, const std::wstring& t, std::function<void()> a, bool solid) {
        owned_.push_back(Button{{x, y, x + w, y + h}, t, a, solid, false});
        return &owned_.back();
    }
    Button* mkLink(int x, int y, int w, const std::wstring& t, std::function<void()> a) {
        owned_.push_back(Button{{x, y, x + w, y + 24}, t, a, false, true});
        return &owned_.back();
    }
    std::list<Button> owned_;

    void drawField(HDC dc, Field& f) {
        txt(dc, 12, false, C_MUT, f.hint, f.x, f.y - 20);
        rr(dc, f.x, f.y, f.w, f.h, 10,
           C_PANEL2, focused == &f ? C_ACC : C_LINE);
        int ix = 8;
        if (!f.icon.empty()) {
            icon(dc, f.icon, f.x + 12, f.y + (f.h - 16) / 2, 16, C_DIM);
            ix = 38;
        }
        std::wstring v = f.pass ? std::wstring(f.text.size(), L'•') : f.text;
        txt(dc, 14, false, C_TXT, v, f.x + ix, f.y + (f.h - 18) / 2);
        if (focused == &f && (GetTickCount() / 530) % 2 == 0) {
            int cx = f.x + ix + tw(dc, 14, false, v);
            HPEN p = CreatePen(PS_SOLID, 2, C_ACC);
            HGDIOBJ o = SelectObject(dc, p);
            MoveToEx(dc, cx, f.y + 10, nullptr);
            LineTo(dc, cx, f.y + f.h - 10);
            SelectObject(dc, o);
            DeleteObject(p);
        }
    }

    // ----- message handling -----
    std::vector<Field*> visibleFields() {
        std::vector<Field*> fs;
        if (screen == Screen::LOGIN) {
            if (forgotMode) fs = {&recNickF, &recMailF, &recPassF};
            else fs = {&nickF, &passF, &serverF};
        } else if (tab == Tab::SET) fs = {&serverF};
        return fs;
    }

    LRESULT proc(UINT m, WPARAM w, LPARAM l) {
        switch (m) {
        case WM_NCHITTEST: {
            int x = GET_X_LPARAM(l) - rcWnd().left, y = GET_Y_LPARAM(l) - rcWnd().top;
            if (y < TITLE_H && x < WIN_W - 100) return HTCAPTION;
            return HTCLIENT;
        }
        case WM_LBUTTONDOWN: {
            int x = GET_X_LPARAM(l), y = GET_Y_LPARAM(l);
            SetFocus(hwnd_);
            if (titleBtn(x, y) == 1) { DestroyWindow(hwnd_); return 0; }
            if (titleBtn(x, y) == 2) { ShowWindow(hwnd_, SW_MINIMIZE); return 0; }
            if (ramSliderRc_.left && x >= ramSliderRc_.left - 10 && x <= ramSliderRc_.right + 10
                && y >= ramSliderRc_.top - 14 && y <= ramSliderRc_.bottom + 14) {
                sliderDrag_ = true;
                sliderFromX(x);
                return 0;
            }
            for (auto it = buttons.rbegin(); it != buttons.rend(); ++it)
                if ((*it)->hit(x, y)) { (*it)->act(); InvalidateRect(hwnd_, nullptr, FALSE); return 0; }
            focused = nullptr;
            for (auto f : visibleFields())
                if (f->hit(x, y)) { focused = f; break; }
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        case WM_LBUTTONUP: sliderDrag_ = false; return 0;
        case WM_MOUSEMOVE:
            mx = GET_X_LPARAM(l); my = GET_Y_LPARAM(l);
            if (sliderDrag_) sliderFromX(mx);
            return 0;
        case WM_MOUSEWHEEL:
            scrollY += GET_WHEEL_DELTA_WPARAM(w) > 0 ? -42 : 42;
            if (scrollY < 0) scrollY = 0;
            return 0;
        case WM_CHAR: {
            if (focused && w >= 32 && focused->text.size() < focused->max) {
                focused->text += (wchar_t)w;
                InvalidateRect(hwnd_, nullptr, FALSE);
            }
            return 0;
        }
        case WM_KEYDOWN:
            if (w == VK_BACK && focused && !focused->text.empty()) {
                focused->text.pop_back();
                InvalidateRect(hwnd_, nullptr, FALSE);
            } else if (w == VK_ESCAPE) { focused = nullptr; forgotMode = false; InvalidateRect(hwnd_, nullptr, FALSE); }
            else if (w == VK_RETURN) { defaultAction(); }
            else if (w == VK_TAB) {
                auto fs = visibleFields();
                if (!fs.empty()) {
                    auto it = std::find(fs.begin(), fs.end(), focused);
                    focused = fs[(it == fs.end() ? 0 : (it - fs.begin() + 1) % fs.size())];
                    InvalidateRect(hwnd_, nullptr, FALSE);
                }
            }
            return 0;
        case WM_TIMER: InvalidateRect(hwnd_, nullptr, FALSE); return 0;
        case WM_APP + 1: InvalidateRect(hwnd_, nullptr, FALSE); return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(hwnd_, &ps);
            HDC mem = CreateCompatibleDC(dc);
            HBITMAP bmp = CreateCompatibleBitmap(dc, WIN_W, WIN_H);
            HGDIOBJ ob = SelectObject(mem, bmp);
            paint(mem);
            BitBlt(dc, 0, 0, WIN_W, WIN_H, mem, 0, 0, SRCCOPY);
            SelectObject(mem, ob);
            DeleteObject(bmp);
            DeleteDC(mem);
            EndPaint(hwnd_, &ps);
            return 0;
        }
        case WM_DESTROY: PostQuitMessage(0); return 0;
        }
        return DefWindowProcW(hwnd_, m, w, l);
    }

    RECT rcWnd() { RECT r; GetWindowRect(hwnd_, &r); r.right -= r.left; r.bottom -= r.top; r.left = r.top = 0; return r; }
    int titleBtn(int x, int y) { // 1 close, 2 min, 0 none
        if (y < 8 || y > 32) return 0;
        if (x > WIN_W - 44) return 1;
        if (x > WIN_W - 84) return 2;
        return 0;
    }
    RECT ramSliderRc_{};
    bool sliderDrag_ = false;
    void sliderFromX(int x) {
        double k = double(x - ramSliderRc_.left) / double(std::max(1L, ramSliderRc_.right - ramSliderRc_.left));
        ramGb = std::min(16, std::max(1, int(1 + k * 15 + 0.5)));
    }
    void defaultAction() {
        if (screen == Screen::LOGIN) {
            if (forgotMode) { if (onRecover) onRecover(); }
            else if (onLogin) onLogin();
        }
    }

public:
    void rebuild() { InvalidateRect(hwnd_, nullptr, FALSE); }

    // ----- paint -----
private:
    void paint(HDC dc) {
        SetBkMode(dc, TRANSPARENT);
        // bg
        {
            HBRUSH b = CreateSolidBrush(C_BG);
            RECT r{0, 0, WIN_W, WIN_H};
            FillRect(dc, &r, b);
            DeleteObject(b);
        }
        DWORD t = GetTickCount() - tick0_;
        int gx = int(WIN_W * 0.85 + sin(t / 2400.0) * 60);
        int gy = int(WIN_H * 0.12 + cos(t / 3100.0) * 40);
        for (int i = 5; i > 0; i--) {
            HBRUSH b = CreateSolidBrush(RGB(10, 12, 11));
            HPEN p = CreatePen(PS_SOLID, 1, RGB(14, 26, 21));
            HGDIOBJ ob = SelectObject(dc, b), op = SelectObject(dc, p);
            Ellipse(dc, gx - i * 34, gy - i * 34, gx + i * 34, gy + i * 34);
            SelectObject(dc, ob); SelectObject(dc, op);
            DeleteObject(b); DeleteObject(p);
        }
        HPEN gp = CreatePen(PS_SOLID, 1, RGB(16, 24, 21));
        HGDIOBJ ogp = SelectObject(dc, gp);
        for (int x = 0; x < WIN_W; x += 72) { MoveToEx(dc, x, 0, nullptr); LineTo(dc, x, WIN_H); }
        for (int y = 0; y < WIN_H; y += 72) { MoveToEx(dc, 0, y, nullptr); LineTo(dc, WIN_W, y); }
        SelectObject(dc, ogp);
        DeleteObject(gp);

        buttons.clear();
        owned_.clear();
        // titlebar
        txt(dc, 15, true, C_TXT, L"ETHEREAL", 20, 11);
        txt(dc, 11, false, C_ACC, L"LAUNCHER", 108, 15);
        int tb = titleBtn(mx, my);
        txt(dc, 15, false, tb == 2 ? C_TXT : C_MUT, L"–", WIN_W - 72, 10);
        txt(dc, 15, false, tb == 1 ? C_RED : C_MUT, L"✕", WIN_W - 34, 10);

        if (screen == Screen::LOGIN) paintLogin(dc);
        else paintMain(dc);
        paintToasts(dc);
    }

    void paintLogin(HDC dc) {
        int w = 400, x = (WIN_W - w) / 2;
        if (!forgotMode) {
            txt(dc, 28, true, C_TXT, L"Ethereal", (WIN_W - tw(dc, 28, true, L"Ethereal")) / 2, 96);
            std::wstring s = L"Welcome back!";
            txt(dc, 14, false, C_MUT, s, (WIN_W - tw(dc, 14, false, s)) / 2, 132);
            int y = 176;
            nickF.x = x; nickF.y = y; nickF.w = w; nickF.h = 48; drawField(dc, nickF);
            passF.x = x; passF.y = y + 74; passF.w = w; passF.h = 48; drawField(dc, passF);
            serverF.x = x; serverF.y = y + 148; serverF.w = w; serverF.h = 48; drawField(dc, serverF);
            drawButton(dc, *mkBtn(x, y + 212, w, 50, L"Sign In", [&] { if (onLogin) onLogin(); }, true));
            drawButton(dc, *mkLink(x, y + 272, w, L"Forgot password?", [&] { forgotMode = true; }));
            std::wstring st = serverOk ? L"сервер на связи" : L"сервер недоступен";
            txt(dc, 11, false, serverOk ? C_ACC : C_DIM, st, (WIN_W - tw(dc, 11, false, st)) / 2, 560);
        } else {
            txt(dc, 28, true, C_TXT, L"Восстановление", (WIN_W - tw(dc, 28, true, L"Восстановление")) / 2, 96);
            std::wstring s = L"ник + почта с регистрации";
            txt(dc, 14, false, C_MUT, s, (WIN_W - tw(dc, 14, false, s)) / 2, 132);
            int y = 176;
            recNickF.x = x; recNickF.y = y; recNickF.w = w; recNickF.h = 48; drawField(dc, recNickF);
            recMailF.x = x; recMailF.y = y + 74; recMailF.w = w; recMailF.h = 48; drawField(dc, recMailF);
            recPassF.x = x; recPassF.y = y + 148; recPassF.w = w; recPassF.h = 48; drawField(dc, recPassF);
            drawButton(dc, *mkBtn(x, y + 212, w, 50, L"Задать новый пароль", [&] { if (onRecover) onRecover(); }, true));
            drawButton(dc, *mkLink(x, y + 272, w, L"Назад ко входу", [&] { forgotMode = false; }));
        }
    }

    void drawLogo(HDC dc, int x, int y, int w, int h) {
        if (!logoBmp_) return;
        Gdiplus::Graphics gg(dc);
        gg.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        gg.DrawImage(logoBmp_, x, y, w, h);
    }

    void paintMain(HDC dc) {
        // slim icon sidebar (reference look)
        HBRUSH sb = CreateSolidBrush(C_PANEL);
        RECT r{0, TITLE_H, 64, WIN_H};
        FillRect(dc, &r, sb);
        DeleteObject(sb);
        HPEN p = CreatePen(PS_SOLID, 1, C_LINE);
        HGDIOBJ o = SelectObject(dc, p);
        MoveToEx(dc, 64, TITLE_H, nullptr); LineTo(dc, 64, WIN_H);
        SelectObject(dc, o); DeleteObject(p);

        drawLogo(dc, 12, TITLE_H + 10, 40, 40);
        const wchar_t* icons[] = {L"home", L"news", L"user", L"gear"};
        const wchar_t* tips[] = {L"Главная", L"Новости", L"Профиль", L"Настройки"};
        Tab vals[] = {Tab::PLAY, Tab::NEWS, Tab::PROFILE, Tab::SET};
        for (int i = 0; i < 4; i++) {
            int ty = TITLE_H + 100 + i * 56;
            if (tab == vals[i]) {
                HBRUSH hb = CreateSolidBrush(RGB(20, 32, 28));
                HGDIOBJ ob = SelectObject(dc, hb);
                RoundRect(dc, 10, ty - 20, 54, ty + 20, 20, 20);
                SelectObject(dc, ob); DeleteObject(hb);
            }
            Tab t = vals[i];
            buttons.push_back(mkBtnRaw(10, ty - 20, 44, 40, L"", [this, t] { tab = t; scrollY = 0; verOpen = false; }));
            icon(dc, icons[i], 22, ty - 9, 20, tab == vals[i] ? C_ACC : C_MUT);
            if (mx >= 10 && mx <= 54 && my >= ty - 20 && my <= ty + 20)
                txt(dc, 11, false, C_TXT, tips[i], 70, ty + 4);
        }
        // bottom: avatar + status
        {
            HBRUSH hb = CreateSolidBrush(C_PANEL2);
            HGDIOBJ ob = SelectObject(dc, hb);
            Ellipse(dc, 16, WIN_H - 48, 48, WIN_H - 16);
            SelectObject(dc, ob); DeleteObject(hb);
            std::wstring ini = hasMe ? std::wstring(1, towupper(me.at(L"name").s()[0])) : L"?";
            txt(dc, 15, true, C_ACC, ini, 32 - tw(dc, 15, true, ini) / 2, WIN_H - 39);
            bool ready = hasMe && me.at(L"subUntil").i() * 1000LL > nowMs();
            HBRUSH db = CreateSolidBrush(ready ? C_ACC : C_DIM);
            HGDIOBJ od = SelectObject(dc, db);
            Ellipse(dc, 66, WIN_H - 30, 74, WIN_H - 22);
            SelectObject(dc, od); DeleteObject(db);
            txt(dc, 11, false, ready ? C_ACC : C_DIM,
                ready ? L"Ready" : (hasMe ? L"Нет подписки" : L"Не в сети"), 80, WIN_H - 24);
        }

        switch (tab) {
        case Tab::PLAY: paintPlay(dc); break;
        case Tab::NEWS: paintNews(dc); break;
        case Tab::PROFILE: paintProfile(dc); break;
        case Tab::SET: paintSet(dc); break;
        }
    }

    Button* mkBtnRaw(int x, int y, int w, int h, const std::wstring& t, std::function<void()> a) {
        owned_.push_back(Button{{x, y, x + w, y + h}, t, a, false, false});
        return &owned_.back();
    }

    void checkRow(HDC dc, int x, int y, bool ok, const std::wstring& label, const std::wstring& value) {
        HBRUSH b = CreateSolidBrush(ok ? C_ACC : C_RED);
        HGDIOBJ o = SelectObject(dc, b);
        Ellipse(dc, x - 4, y - 8, x + 4, y);
        SelectObject(dc, o); DeleteObject(b);
        txt(dc, 14, false, C_TXT, label, x + 16, y - 14);
        if (!value.empty()) txt(dc, 11, false, C_MUT, value, x + 16, y + 6);
    }

    static std::wstring fmtLeft(long long ms) {
        long long d = ms / 86400000, h = ms % 86400000 / 3600000;
        if (d > 0) return std::to_wstring(d) + L" дн. " + std::to_wstring(h) + L" ч.";
        return std::to_wstring(h) + L" ч.";
    }

    void paintPlay(HDC dc) {
        int cx = (64 + WIN_W) / 2;
        // big logo
        int lw = 300, lh = 300;
        drawLogo(dc, cx - lw / 2, 92, lw, lh);
        // lowercase brand with tracking
        std::wstring brand = L"ethereal";
        HFONT f = font(26, false);
        HGDIOBJ of = SelectObject(dc, f);
        SetTextColor(dc, C_ACC);
        SetBkMode(dc, TRANSPARENT);
        SetTextCharacterExtra(dc, 14);
        SIZE zb{};
        GetTextExtentPoint32W(dc, brand.c_str(), (int)brand.size(), &zb);
        int extra = 14 * ((int)brand.size() - 1);
        TextOutW(dc, cx - (zb.cx + extra) / 2, 400, brand.c_str(), (int)brand.size());
        SetTextCharacterExtra(dc, 0);
        SelectObject(dc, of);

        bool subOk = hasMe && me.at(L"subUntil").i() * 1000LL > nowMs();
        // glowing launch pill
        int bw = 320, bh = 58, bx = cx - bw / 2, by = 452;
        for (int i = 6; i > 0; i--) {
            HBRUSH gb = CreateSolidBrush(RGB(10, 12, 11));
            HPEN gp = CreatePen(PS_SOLID, 1, RGB(20 + i * 6, 60 + i * 12, 45 + i * 9));
            HGDIOBJ ogb = SelectObject(dc, gb), ogp = SelectObject(dc, gp);
            RoundRect(dc, bx - i * 2, by - i * 2, bx + bw + i * 2, by + bh + i * 2, 60, 60);
            SelectObject(dc, ogb); SelectObject(dc, ogp);
            DeleteObject(gb); DeleteObject(gp);
        }
        Button* launch = mkBtn(bx, by, bw, bh, L"Запустить  ›", [&] { if (onLaunch) onLaunch(); }, true);
        launch->enabled = dlProgress < 0;
        drawButton(dc, *launch);
        if (!hasMe)
            txt(dc, 11, false, C_DIM, L"войди: данные подтянутся с сайта", cx - 120, by + 66);
        else if (!subOk)
            txt(dc, 11, false, C_GOLD, L"нет подписки — оформи на сайте", cx - 110, by + 66);

        // version pill + dropdown (only 1.21.11 for now)
        int vw = 210, vh = 38, vx = cx - vw / 2, vy = by + 92;
        rr(dc, vx, vy, vw, vh, 19, C_PANEL, C_LINE);
        txt(dc, 13, false, C_ACC, versions[verSel], vx + 20, vy + 11);
        txt(dc, 14, false, C_ACC, verOpen ? L"∧" : L"∨", vx + vw - 32, vy + 10);
        buttons.push_back(mkBtnRaw(vx, vy, vw, vh, L"", [&] { verOpen = !verOpen; }));
        if (verOpen) {
            for (size_t i = 0; i < versions.size(); i++) {
                int iy = vy + vh + 6 + (int)i * 38;
                rr(dc, vx, iy, vw, 34, 17, C_PANEL2, i == (size_t)verSel ? C_ACC : C_LINE);
                txt(dc, 13, false, i == (size_t)verSel ? C_ACC : C_TXT, versions[i], vx + 20, iy + 9);
                int ii = (int)i;
                buttons.push_back(mkBtnRaw(vx, iy, vw, 34, L"", [this, ii] { verSel = ii; verOpen = false; }));
            }
        }
        // progress / status under
        if (dlProgress >= 0) {
            int pby = vy + (verOpen ? (int)versions.size() * 38 + 20 : 60);
            rr(dc, cx - 200, pby, 400, 8, 4, C_PANEL2, C_PANEL2, false);
            HBRUSH b = CreateSolidBrush(C_ACC);
            HGDIOBJ oo = SelectObject(dc, b);
            RoundRect(dc, cx - 200, pby, cx - 200 + int(400 * dlProgress / 100.0), pby + 8, 8, 8);
            SelectObject(dc, oo); DeleteObject(b);
            std::wstring pl = statusLine.empty() ? L"работаю" : statusLine;
            txt(dc, 11, false, C_MUT, pl + L"… " + std::to_wstring(dlProgress) + L"%",
                cx - tw(dc, 11, false, pl) / 2, pby + 16);
        } else if (!statusLine.empty()) {
            txt(dc, 11, false, C_MUT, statusLine, cx - tw(dc, 11, false, statusLine) / 2, vy + 60);
        }
        if (!lastLaunched.empty())
            txt(dc, 11, false, C_ACC, lastLaunched, cx - tw(dc, 11, false, lastLaunched) / 2, WIN_H - 56);
    }

    static bool starts(const std::wstring& s, const wchar_t* p) {
        return s.rfind(p, 0) == 0;
    }
    static long long nowMs() {
        FILETIME ft; GetSystemTimeAsFileTime(&ft);
        ULARGE_INTEGER u; u.LowPart = ft.dwLowDateTime; u.HighPart = ft.dwHighDateTime;
        return (long long)(u.QuadPart / 10000) - 11644473600000LL;
    }

    std::vector<std::wstring> wrap(HDC dc, int px, bool bold, const std::wstring& s, int maxW) {
        std::vector<std::wstring> lines;
        HFONT f = font(px, bold);
        HGDIOBJ o = SelectObject(dc, f);
        SIZE z{};
        auto wd = [&](const std::wstring& t) { GetTextExtentPoint32W(dc, t.c_str(), (int)t.size(), &z); return z.cx; };
        size_t start = 0;
        for (size_t i = 0; i <= s.size(); i++) {
            if (i == s.size() || s[i] == L'\n' || s[i] == L' ') {
                // greedy wrap below instead
            }
        }
        // word wrap
        std::wstring cur;
        size_t i = 0;
        auto flush = [&] { if (!cur.empty()) lines.push_back(cur); cur.clear(); };
        while (i < s.size()) {
            if (s[i] == L'\n') { flush(); i++; continue; }
            size_t j = s.find_first_of(L" \n", i);
            if (j == std::wstring::npos) j = s.size();
            std::wstring word = s.substr(i, j - i);
            std::wstring t = cur.empty() ? word : cur + L" " + word;
            if (wd(t) > maxW && !cur.empty()) { flush(); cur = word; }
            else cur = t;
            i = j + (j < s.size() && s[j] == L' ' ? 1 : 0);
            if (j < s.size() && s[j] == L'\n') { flush(); i = j + 1; }
        }
        flush();
        (void)start;
        SelectObject(dc, o);
        return lines;
    }

    void paintNews(HDC dc) {
        int x = 110;
        txt(dc, 28, true, C_TXT, L"Новости", x, 62);
        int y = 116 - std::min(scrollY, 4000);
        if (!buildsErr.empty() && builds.type != json::Value::ARR) {
            txt(dc, 14, false, C_MUT, L"Не вышло загрузить: " + buildsErr, x, y);
            return;
        }
        if (builds.type == json::Value::ARR) {
            for (auto& bo : builds.arr) {
                auto lines = wrap(dc, 14, false, bo.at(L"text").s(), 640);
                int hh = 70 + (int)lines.size() * 18;
                if (y + hh > 96 && y < WIN_H - 20) {
                    rr(dc, x, y, 680, hh, 14, C_PANEL, C_LINE);
                    txt(dc, 15, true, C_ACC, bo.at(L"version").s(), x + 18, y + 12);
                    std::wstring dt = bo.at(L"date").s();
                    txt(dc, 11, false, C_DIM, dt, x + 662 - tw(dc, 11, false, dt), y + 14);
                    txt(dc, 14, false, C_TXT, bo.at(L"title").s(), x + 18, y + 36);
                    int ly = y + 58;
                    for (auto& ln : lines) { txt(dc, 14, false, C_MUT, ln, x + 18, ly); ly += 18; }
                }
                y += hh + 12;
            }
        }
    }

    void kvRow(HDC dc, int x, int y, const std::wstring& k, const std::wstring& v) {
        txt(dc, 14, false, C_MUT, k, x, y);
        txt(dc, 14, false, C_TXT, v, x + 210, y);
    }

    void paintProfile(HDC dc) {
        int x = 110;
        txt(dc, 28, true, C_TXT, L"Профиль", x, 62);
        if (!hasMe) {
            txt(dc, 14, false, C_MUT, L"Нет входа.", x, 120);
            drawButton(dc, *mkBtn(x, 140, 240, 46, L"К экрану входа", [&] { screen = Screen::LOGIN; }, true));
            return;
        }
        int y = 122;
        kvRow(dc, x, y, L"Ник", me.at(L"name").s()); y += 30;
        kvRow(dc, x, y, L"UID", L"#" + std::to_wstring(me.at(L"uid").i())); y += 30;
        kvRow(dc, x, y, L"Роль", me.at(L"role").s()); y += 30;
        kvRow(dc, x, y, L"Почта", me.at(L"email").s()); y += 30;
        long long left = me.at(L"subUntil").i() * 1000LL - nowMs();
        kvRow(dc, x, y, L"Подписка", left > 0 ? me.at(L"plan").s() + L" · " + fmtLeft(left) : L"нет"); y += 30;
        std::wstring hw = me.at(L"hwid").s();
        kvRow(dc, x, y, L"HWID (сервер)", hw.empty() ? L"привяжется при запуске" : hw); y += 30;
        extern std::wstring g_localHwid();
        kvRow(dc, x, y, L"HWID (этот ПК)", g_localHwid()); y += 30;
        kvRow(dc, x, y, L"Баланс", std::to_wstring(me.at(L"balance").i()) + L" ₽"); y += 54;
        drawButton(dc, *mkBtn(x, y, 240, 46, L"Кабинет на сайте", [&] { if (onOpenCabinet) onOpenCabinet(); }, false));
        drawButton(dc, *mkBtn(x + 254, y, 170, 46, L"Выйти", [&] { if (onLogout) onLogout(); }, false));
    }

    void paintSet(HDC dc) {
        int x = 110;
        txt(dc, 28, true, C_TXT, L"Настройки", x, 62);
        serverF.x = x; serverF.y = 128; serverF.w = 560; serverF.h = 44; drawField(dc, serverF);
        cmdF.x = x; cmdF.y = 200; cmdF.w = 480; cmdF.h = 44; drawField(dc, cmdF);
        txt(dc, 11, false, C_DIM, L"пустая команда = автовыбор. {RAM} подставится из слайдера", x, 258);

        txt(dc, 14, false, C_TXT, L"Оперативная память: " + std::to_wstring(ramGb) + L" ГБ", x, 296);
        int sx = x, sy = 322, sw = 400;
        ramSliderRc_ = {sx, sy - 4, sx + sw, sy + 4};
        rr(dc, sx, sy - 4, sw, 8, 4, C_PANEL2, C_PANEL2, false);
        int kx = sx + int(sw * (ramGb - 1) / 15.0);
        {
            HBRUSH b = CreateSolidBrush(C_ACC);
            HGDIOBJ oo = SelectObject(dc, b);
            RoundRect(dc, sx, sy - 4, kx + 2, sy + 4, 8, 8);
            SelectObject(dc, oo); DeleteObject(b);
        }
        {
            HBRUSH b = CreateSolidBrush(C_ACC);
            HGDIOBJ oo = SelectObject(dc, b);
            Ellipse(dc, kx - 10, sy - 10, kx + 10, sy + 10);
            SelectObject(dc, oo); DeleteObject(b);
        }
        icon(dc, L"ram", sx + sw + 16, sy - 10, 20, C_DIM);

        drawButton(dc, *mkBtn(x, 360, 230, 46, L"Сохранить", [&] { if (onSaveSettings) onSaveSettings(); }, true));
        drawButton(dc, *mkBtn(x + 244, 360, 260, 46, L"Проверить соединение", [&] { if (onTestConn) onTestConn(); }, false));
    }

    void paintToasts(HDC dc) {
        DWORD now = GetTickCount();
        toasts.erase(std::remove_if(toasts.begin(), toasts.end(),
            [&](const Toast& t) { return now > t.until; }), toasts.end());
        int y = WIN_H - 24;
        for (int i = (int)toasts.size() - 1; i >= 0; i--) {
            auto& t = toasts[i];
            int tww = tw(dc, 14, false, t.text) + 32;
            int tx = WIN_W - tww - 20;
            rr(dc, tx, y - 26, tww, 36, 10, C_PANEL2, t.err ? C_RED : C_ACC);
            txt(dc, 14, false, C_TXT, t.text, tx + 16, y - 20);
            y -= 46;
        }
    }
};

} // namespace ui
