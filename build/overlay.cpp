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
//  Archipelago events over the game. Drawn with GDI on its own thread. Magenta
//  is the transparency key; everything else renders at a constant alpha. The
//  window owns no game state and is wrapped so any failure is silent.
// ===========================================================================

namespace {

constexpr wchar_t   kClassName[] = L"AC6AP_Overlay";
constexpr COLORREF  kKey        = RGB(255, 0, 255);   // transparent color key
constexpr int       kWidth      = 560;
constexpr int       kMaxLines   = 8;
constexpr int       kLineH      = 26;
constexpr int       kPadX       = 12;
constexpr int       kPadY       = 10;
constexpr DWORD     kLifeMs     = 12000;              // how long a message stays

struct Msg {
    std::wstring text;
    COLORREF     color;
    DWORD        born;
};

std::mutex          g_mtx;
std::deque<Msg>     g_msgs;
std::atomic<bool>   g_running{ false };
std::thread         g_thread;
HWND                g_hwnd = nullptr;
HFONT               g_font = nullptr;

COLORREF KindColor(AC6OverlayKind k) {
    switch (k) {
        case OVL_RECEIVED: return RGB(120, 230, 140);  // green
        case OVL_SENT:     return RGB(110, 200, 255);  // cyan
        default:           return RGB(235, 235, 235);  // white
    }
}

// Find this process's main game window (visible, owner-less, titled), skipping
// our own overlay window.
HWND FindGameWindow() {
    struct Ctx { DWORD pid; HWND found; } ctx{ GetCurrentProcessId(), nullptr };
    EnumWindows([](HWND h, LPARAM lp) -> BOOL {
        auto* c = reinterpret_cast<Ctx*>(lp);
        if (h == g_hwnd) return TRUE;
        DWORD pid = 0; GetWindowThreadProcessId(h, &pid);
        if (pid != c->pid) return TRUE;
        if (!IsWindowVisible(h) || GetWindow(h, GW_OWNER) != nullptr) return TRUE;
        if (GetWindowTextLengthW(h) <= 0) return TRUE;
        c->found = h;
        return FALSE;
    }, reinterpret_cast<LPARAM>(&ctx));
    return ctx.found;
}

void DrawShadowText(HDC dc, int x, int y, const std::wstring& s, COLORREF col) {
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(0, 0, 0));
    TextOutW(dc, x + 1, y + 1, s.c_str(), (int)s.size());   // shadow
    SetTextColor(dc, col);
    TextOutW(dc, x, y, s.c_str(), (int)s.size());
}

void Render() {
    if (!g_hwnd) return;

    // Snapshot + expire.
    std::vector<Msg> live;
    DWORD now = GetTickCount();
    {
        std::lock_guard<std::mutex> lk(g_mtx);
        while (!g_msgs.empty() && now - g_msgs.front().born > kLifeMs)
            g_msgs.pop_front();
        for (const auto& m : g_msgs) live.push_back(m);
    }

    int rows   = (int)live.size();
    int height = kPadY * 2 + (rows > 0 ? rows : 1) * kLineH;

    // Anchor to the game window's top-left, with a margin. Fall back to screen.
    RECT gr{ 0, 0, 0, 0 };
    HWND game = FindGameWindow();
    if (game) GetClientRect(game, &gr);
    POINT origin{ 40, 40 };
    if (game) { POINT p{ 24, 90 }; ClientToScreen(game, &p); origin = p; }

    SetWindowPos(g_hwnd, HWND_TOPMOST, origin.x, origin.y, kWidth, height,
                 SWP_NOACTIVATE | SWP_NOREDRAW);

    HDC     wdc = GetDC(g_hwnd);
    HDC     mdc = CreateCompatibleDC(wdc);
    HBITMAP bmp = CreateCompatibleBitmap(wdc, kWidth, height);
    HBITMAP old = (HBITMAP)SelectObject(mdc, bmp);
    HFONT   ofn = (HFONT)SelectObject(mdc, g_font);

    HBRUSH keyBrush = CreateSolidBrush(kKey);
    RECT full{ 0, 0, kWidth, height };
    FillRect(mdc, &full, keyBrush);              // transparent background

    if (rows > 0) {
        HBRUSH bar = CreateSolidBrush(RGB(18, 18, 22));   // dark backing bar
        RECT br{ 0, 0, kWidth, height };
        FillRect(mdc, &br, bar);
        DeleteObject(bar);
        int y = kPadY;
        for (const auto& m : live) {
            DrawShadowText(mdc, kPadX, y, m.text, m.color);
            y += kLineH;
        }
    }

    BitBlt(wdc, 0, 0, kWidth, height, mdc, 0, 0, SRCCOPY);

    SelectObject(mdc, ofn);
    SelectObject(mdc, old);
    DeleteObject(bmp);
    DeleteObject(keyBrush);
    DeleteDC(mdc);
    ReleaseDC(g_hwnd, wdc);
}

LRESULT CALLBACK WndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProcW(h, msg, wp, lp);
}

void ThreadMain() {
    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = GetModuleHandleW(nullptr);
    wc.lpszClassName = kClassName;
    RegisterClassExW(&wc);

    g_hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST |
        WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        kClassName, L"AC6AP Overlay", WS_POPUP,
        40, 40, kWidth, kPadY * 2 + kLineH, nullptr, nullptr,
        wc.hInstance, nullptr);
    if (!g_hwnd) { Log("Overlay: CreateWindowEx failed (%lu)", GetLastError()); return; }

    // Magenta = transparent; everything else at constant alpha.
    SetLayeredWindowAttributes(g_hwnd, kKey, 225, LWA_COLORKEY | LWA_ALPHA);

    g_font = CreateFontW(-18, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                         DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                         CLEARTYPE_QUALITY, FF_DONTCARE, L"Segoe UI");

    ShowWindow(g_hwnd, SW_SHOWNOACTIVATE);
    Log("Overlay window created");

    while (g_running) {
        MSG m;
        while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&m);
            DispatchMessageW(&m);
        }
        Render();
        Sleep(100);   // ~10 fps is plenty for a text feed
    }

    if (g_font) { DeleteObject(g_font); g_font = nullptr; }
    if (g_hwnd) { DestroyWindow(g_hwnd); g_hwnd = nullptr; }
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

    // UTF-8/ANSI -> wide for GDI.
    wchar_t wbuf[512];
    int n = MultiByteToWideChar(CP_UTF8, 0, buf, -1, wbuf, 512);
    if (n <= 0) MultiByteToWideChar(CP_ACP, 0, buf, -1, wbuf, 512);

    std::lock_guard<std::mutex> lk(g_mtx);
    g_msgs.push_back({ wbuf, KindColor(kind), GetTickCount() });
    while (g_msgs.size() > (size_t)kMaxLines) g_msgs.pop_front();
}
