#include "overlay.h"
#include "dllmain.h"   // Log()

#include <windows.h>
#include <thread>
#include <atomic>
#include <mutex>
#include <deque>
#include <string>
#include <vector>
#include <cstdarg>
#include <cstdio>

// ===========================================================================
//  A passive, click-through, top-most layered window that lists recent
//  Archipelago events over the game. Drawn with GDI on its own thread.
//
//  Design rules that keep it from ever hanging or disturbing the game:
//   - NEVER send messages to other windows (no GetWindowText / SendMessage).
//     The game window is located with GetWindowRect/EnumWindows only, which do
//     not block on another thread. (An earlier version used GetWindowTextLength
//     here and hung the overlay during loads.)
//   - GDI objects (font, back-buffer DC + bitmap) are created ONCE and reused.
//   - Magenta is the transparency color key; the window's class background is
//     also magenta so it is transparent even before the first paint.
//   - Render does no allocation that can throw inside the GDI section.
// ===========================================================================

namespace {

constexpr wchar_t   kClassName[] = L"AC6AP_Overlay";
constexpr COLORREF  kKey        = RGB(255, 0, 255);   // transparent color key
constexpr int       kWidth      = 600;
constexpr int       kMaxLines   = 8;
constexpr int       kLineH      = 24;
constexpr int       kPadX       = 10;
constexpr int       kPadY       = 8;
constexpr int       kHeight     = kPadY * 2 + kMaxLines * kLineH;
constexpr DWORD     kLifeMs     = 12000;              // how long a message stays

struct Msg { std::wstring text; COLORREF color; DWORD born; };

std::mutex          g_mtx;
std::deque<Msg>     g_msgs;
std::atomic<bool>   g_running{ false };
std::thread         g_thread;
HWND                g_hwnd   = nullptr;
HFONT               g_font   = nullptr;
HDC                 g_memDC  = nullptr;   // cached back buffer
HBITMAP             g_memBmp = nullptr;
HBRUSH              g_keyBr  = nullptr;   // magenta = transparent
HBRUSH              g_panBr  = nullptr;   // dark backing panel
HWND                g_game   = nullptr;   // cached game window (may be null)

COLORREF KindColor(AC6OverlayKind k) {
    switch (k) {
        case OVL_RECEIVED: return RGB(120, 230, 140);  // green
        case OVL_SENT:     return RGB(110, 200, 255);  // cyan
        default:           return RGB(240, 240, 240);  // white
    }
}

// Pick this process's largest visible owner-less window = the game window.
// Uses only non-blocking calls (no message is sent to any window).
BOOL CALLBACK EnumProc(HWND h, LPARAM lp) {
    if (h == g_hwnd) return TRUE;
    DWORD pid = 0; GetWindowThreadProcessId(h, &pid);
    if (pid != GetCurrentProcessId()) return TRUE;
    if (!IsWindowVisible(h) || GetWindow(h, GW_OWNER) != nullptr) return TRUE;
    RECT r; if (!GetWindowRect(h, &r)) return TRUE;
    long area = (r.right - r.left) * (long)(r.bottom - r.top);
    auto* best = reinterpret_cast<std::pair<HWND, long>*>(lp);
    if (area > best->second) { best->second = area; best->first = h; }
    return TRUE;
}
HWND FindGameWindow() {
    std::pair<HWND, long> best{ nullptr, 0 };
    EnumWindows(EnumProc, reinterpret_cast<LPARAM>(&best));
    return best.first;
}

void Render() {
    if (!g_hwnd || !g_memDC) return;

    // Expire + snapshot (small, fixed cap).
    Msg live[kMaxLines];
    int n = 0;
    DWORD now = GetTickCount();
    {
        std::lock_guard<std::mutex> lk(g_mtx);
        while (!g_msgs.empty() && now - g_msgs.front().born > kLifeMs)
            g_msgs.pop_front();
        for (const auto& m : g_msgs) { if (n < kMaxLines) live[n++] = m; }
    }

    // Anchor to the game window's top-left if we have it (no blocking calls).
    if (!g_game || !IsWindow(g_game)) g_game = FindGameWindow();
    int x = 40, y = 40;
    RECT gr;
    if (g_game && GetWindowRect(g_game, &gr)) { x = gr.left + 24; y = gr.top + 90; }
    SetWindowPos(g_hwnd, nullptr, x, y, kWidth, kHeight,
                 SWP_NOZORDER | SWP_NOACTIVATE);

    // Magenta everywhere (transparent), then an opaque dark panel sized to the
    // text, then the text on top. The window-wide LWA_ALPHA keeps the panel a
    // touch translucent; only the panel area is visible (magenta is keyed out).
    RECT full{ 0, 0, kWidth, kHeight };
    FillRect(g_memDC, &full, g_keyBr);

    if (n > 0) {
        int maxW = 0;
        for (int i = 0; i < n; i++) {
            SIZE sz;
            GetTextExtentPoint32W(g_memDC, live[i].text.c_str(),
                                  (int)live[i].text.size(), &sz);
            if (sz.cx > maxW) maxW = sz.cx;
        }
        RECT panel{ 0, 0, (maxW + kPadX * 2 > kWidth) ? kWidth : maxW + kPadX * 2,
                    n * kLineH + kPadY * 2 };
        FillRect(g_memDC, &panel, g_panBr);

        SetBkMode(g_memDC, TRANSPARENT);
        int ty = kPadY;
        for (int i = 0; i < n; i++) {
            SetTextColor(g_memDC, live[i].color);
            TextOutW(g_memDC, kPadX, ty, live[i].text.c_str(), (int)live[i].text.size());
            ty += kLineH;
        }
    }

    HDC wdc = GetDC(g_hwnd);
    if (wdc) {
        BitBlt(wdc, 0, 0, kWidth, kHeight, g_memDC, 0, 0, SRCCOPY);
        ReleaseDC(g_hwnd, wdc);
    }
}

LRESULT CALLBACK WndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProcW(h, msg, wp, lp);
}

void ThreadMain() {
    g_keyBr = CreateSolidBrush(kKey);
    g_panBr = CreateSolidBrush(RGB(16, 16, 22));   // dark backing panel

    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = GetModuleHandleW(nullptr);
    wc.lpszClassName = kClassName;
    wc.hbrBackground = g_keyBr;   // transparent before first paint, never black
    RegisterClassExW(&wc);

    g_hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST |
        WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        kClassName, L"AC6AP Overlay", WS_POPUP,
        40, 40, kWidth, kHeight, nullptr, nullptr, wc.hInstance, nullptr);
    if (!g_hwnd) { Log("Overlay: CreateWindowEx failed (%lu)", GetLastError()); return; }

    // Magenta keyed transparent; everything else (the dark panel + text) at a
    // high constant alpha so the panel is opaque enough to read over the game.
    SetLayeredWindowAttributes(g_hwnd, kKey, 236, LWA_COLORKEY | LWA_ALPHA);

    g_font = CreateFontW(-17, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                         DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                         ANTIALIASED_QUALITY, FF_DONTCARE, L"Segoe UI");

    HDC screen = GetDC(nullptr);
    g_memDC  = CreateCompatibleDC(screen);
    g_memBmp = CreateCompatibleBitmap(screen, kWidth, kHeight);
    ReleaseDC(nullptr, screen);
    SelectObject(g_memDC, g_memBmp);
    SelectObject(g_memDC, g_font);

    SetWindowPos(g_hwnd, HWND_TOPMOST, 40, 40, kWidth, kHeight,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
    Log("Overlay window created");

    while (g_running) {
        MSG m;
        while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&m);
            DispatchMessageW(&m);
        }
        Render();
        Sleep(120);
    }

    if (g_memBmp) { DeleteObject(g_memBmp); g_memBmp = nullptr; }
    if (g_memDC)  { DeleteDC(g_memDC);      g_memDC  = nullptr; }
    if (g_font)   { DeleteObject(g_font);   g_font   = nullptr; }
    if (g_hwnd)   { DestroyWindow(g_hwnd);  g_hwnd   = nullptr; }
    if (g_keyBr)  { DeleteObject(g_keyBr);  g_keyBr  = nullptr; }
    if (g_panBr)  { DeleteObject(g_panBr);  g_panBr  = nullptr; }
    UnregisterClassW(kClassName, GetModuleHandleW(nullptr));
}

} // namespace

void Overlay_Start() {
    if (g_running.exchange(true)) return;
    g_thread = std::thread(ThreadMain);
}

void Overlay_Stop() {
    if (!g_running.exchange(false)) return;
    if (g_hwnd) PostMessageW(g_hwnd, WM_CLOSE, 0, 0);
    if (g_thread.joinable()) g_thread.join();
}

void Overlay_Message(AC6OverlayKind kind, const char* fmt, ...) {
    char buf[512];
    va_list args; va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    wchar_t wbuf[512];
    if (MultiByteToWideChar(CP_UTF8, 0, buf, -1, wbuf, 512) <= 0)
        MultiByteToWideChar(CP_ACP, 0, buf, -1, wbuf, 512);

    std::lock_guard<std::mutex> lk(g_mtx);
    g_msgs.push_back({ wbuf, KindColor(kind), GetTickCount() });
    while (g_msgs.size() > (size_t)kMaxLines) g_msgs.pop_front();
}
