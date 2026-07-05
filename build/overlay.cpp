#include "overlay.h"
#define AC6AP_LOG_TAG "UI"
#include "dllmain.h"   // Log()

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
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
    constexpr COLORREF  kKey = RGB(255, 0, 255);   // transparent color key
    constexpr int       kMaxLines = 8;                  // fixed; scale grows size, not line count
    constexpr DWORD     kLifeMs = 12000;              // how long a message stays

    // Base (1.0x) layout metrics. Actual pixel metrics below are these times the
    // UI scale, computed once at window creation.
    constexpr int       kBaseWidth = 600;
    constexpr int       kBaseLineH = 24;
    constexpr int       kBasePadX = 10;
    constexpr int       kBasePadY = 8;
    constexpr int       kBaseFont = 17;                 // font cell height (px)

    float               g_scale = 1.0f;               // set via Overlay_SetScale
    int                 g_width = kBaseWidth;
    int                 g_lineH = kBaseLineH;
    int                 g_padX = kBasePadX;
    int                 g_padY = kBasePadY;
    int                 g_height = kBasePadY * 2 + kMaxLines * kBaseLineH;
    int                 g_fontPx = kBaseFont;

    // Recompute the pixel metrics from g_scale. Called at window creation.
    void ComputeMetrics() {
        g_width = (int)(kBaseWidth * g_scale);
        g_lineH = (int)(kBaseLineH * g_scale);
        g_padX = (int)(kBasePadX * g_scale);
        g_padY = (int)(kBasePadY * g_scale);
        g_height = g_padY * 2 + kMaxLines * g_lineH;
        g_fontPx = (int)(kBaseFont * g_scale);
    }

    struct Msg { std::wstring text; COLORREF color; DWORD born; };

    std::mutex          g_mtx;
    std::deque<Msg>     g_msgs;
    std::atomic<bool>   g_running{ false };
    std::thread         g_thread;
    HWND                g_hwnd = nullptr;
    HFONT               g_font = nullptr;
    HDC                 g_memDC = nullptr;   // cached back buffer
    HBITMAP             g_memBmp = nullptr;
    HBRUSH              g_keyBr = nullptr;   // magenta = transparent
    HBRUSH              g_panBr = nullptr;   // dark backing panel
    HWND                g_game = nullptr;   // cached game window (may be null)

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
        SetWindowPos(g_hwnd, nullptr, x, y, g_width, g_height,
            SWP_NOZORDER | SWP_NOACTIVATE);

        // Magenta everywhere (transparent), then an opaque dark panel sized to the
        // text, then the text on top. The window-wide LWA_ALPHA keeps the panel a
        // touch translucent; only the panel area is visible (magenta is keyed out).
        RECT full{ 0, 0, g_width, g_height };
        FillRect(g_memDC, &full, g_keyBr);

        if (n > 0) {
            int maxW = 0;
            for (int i = 0; i < n; i++) {
                SIZE sz;
                GetTextExtentPoint32W(g_memDC, live[i].text.c_str(),
                    (int)live[i].text.size(), &sz);
                if (sz.cx > maxW) maxW = sz.cx;
            }
            RECT panel{ 0, 0, (maxW + g_padX * 2 > g_width) ? g_width : maxW + g_padX * 2,
                        n * g_lineH + g_padY * 2 };
            FillRect(g_memDC, &panel, g_panBr);

            SetBkMode(g_memDC, TRANSPARENT);
            int ty = g_padY;
            for (int i = 0; i < n; i++) {
                SetTextColor(g_memDC, live[i].color);
                TextOutW(g_memDC, g_padX, ty, live[i].text.c_str(), (int)live[i].text.size());
                ty += g_lineH;
            }
        }

        HDC wdc = GetDC(g_hwnd);
        if (wdc) {
            BitBlt(wdc, 0, 0, g_width, g_height, g_memDC, 0, 0, SRCCOPY);
            ReleaseDC(g_hwnd, wdc);
        }
    }

    LRESULT CALLBACK WndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
        if (msg == WM_DESTROY) { PostQuitMessage(0); return 0; }
        return DefWindowProcW(h, msg, wp, lp);
    }

    void ThreadMain() {
        ComputeMetrics();   // resolve pixel sizes from the UI scale

        g_keyBr = CreateSolidBrush(kKey);
        g_panBr = CreateSolidBrush(RGB(16, 16, 22));   // dark backing panel

        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = WndProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = kClassName;
        wc.hbrBackground = g_keyBr;   // transparent before first paint, never black
        RegisterClassExW(&wc);

        g_hwnd = CreateWindowExW(
            WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST |
            WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
            kClassName, L"AC6AP Overlay", WS_POPUP,
            40, 40, g_width, g_height, nullptr, nullptr, wc.hInstance, nullptr);
        if (!g_hwnd) { Log("Overlay: CreateWindowEx failed (%lu)", GetLastError()); return; }

        // Magenta keyed transparent; everything else (the dark panel + text) at a
        // high constant alpha so the panel is opaque enough to read over the game.
        SetLayeredWindowAttributes(g_hwnd, kKey, 236, LWA_COLORKEY | LWA_ALPHA);

        g_font = CreateFontW(-g_fontPx, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
            ANTIALIASED_QUALITY, FF_DONTCARE, L"Segoe UI");

        HDC screen = GetDC(nullptr);
        g_memDC = CreateCompatibleDC(screen);
        g_memBmp = CreateCompatibleBitmap(screen, g_width, g_height);
        ReleaseDC(nullptr, screen);
        SelectObject(g_memDC, g_memBmp);
        SelectObject(g_memDC, g_font);

        SetWindowPos(g_hwnd, HWND_TOPMOST, 40, 40, g_width, g_height,
            SWP_NOACTIVATE | SWP_SHOWWINDOW);
        Log("Overlay window created (scale %.2f)", g_scale);

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
        if (g_memDC) { DeleteDC(g_memDC);      g_memDC = nullptr; }
        if (g_font) { DeleteObject(g_font);   g_font = nullptr; }
        if (g_hwnd) { DestroyWindow(g_hwnd);  g_hwnd = nullptr; }
        if (g_keyBr) { DeleteObject(g_keyBr);  g_keyBr = nullptr; }
        if (g_panBr) { DeleteObject(g_panBr);  g_panBr = nullptr; }
        UnregisterClassW(kClassName, GetModuleHandleW(nullptr));
    }

} // namespace

void Overlay_SetScale(float scale) {
    if (scale < 0.5f) scale = 0.5f;   // clamp to a sane range
    if (scale > 4.0f) scale = 4.0f;
    g_scale = scale;
}

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