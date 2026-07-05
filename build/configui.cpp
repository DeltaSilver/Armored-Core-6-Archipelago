#include "configui.h"
#include "dllmain.h"    // Log(), AC6_ApplyConnection()
#include "apclient.h"   // APClient_StatusText()

#include <windows.h>
#include <uxtheme.h>
#include <thread>
#include <atomic>
#include <string>

#pragma comment(lib, "uxtheme.lib")

// A dark, borderless, top-most panel drawn over the game (no window chrome),
// toggled by F8. It is a real window so it can take keyboard focus for the
// fields, but visually it sits on the game like the message overlay. Shows in
// borderless/windowed mode.

namespace {

constexpr wchar_t kClass[] = L"AC6AP_Settings";
enum { IDC_HOST = 1001, IDC_PORT, IDC_SLOT, IDC_PASS, IDC_TOGGLE, IDC_STATUS };
enum { WM_TOGGLE = WM_APP + 1 };

constexpr int kW = 400, kH = 296;
const COLORREF kBg   = RGB(14, 14, 18);     // panel
const COLORREF kEdit = RGB(34, 34, 42);     // field
const COLORREF kFg   = RGB(232, 232, 238);   // text
const COLORREF kBtn  = RGB(48, 60, 80);      // button
const COLORREF kHint = RGB(150, 150, 160);

std::atomic<bool> g_running{ false };
std::thread       g_thread;
HWND g_wnd = nullptr, g_eHost = nullptr, g_ePort = nullptr,
     g_eSlot = nullptr, g_ePass = nullptr, g_status = nullptr, g_btn = nullptr;
int  g_lastState = -1;   // last connection state reflected on the button
HBRUSH g_bgBr = nullptr, g_editBr = nullptr;
HFONT  g_font = nullptr, g_fontHdr = nullptr;
std::wstring g_iHost, g_iPort, g_iSlot, g_iPass;

std::wstring ToW(const char* s) {
    if (!s) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
    std::wstring w(n > 0 ? n - 1 : 0, 0);
    if (n > 0) MultiByteToWideChar(CP_UTF8, 0, s, -1, &w[0], n);
    return w;
}
std::string ToA(const std::wstring& w) {
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string s(n > 0 ? n - 1 : 0, 0);
    if (n > 0) WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &s[0], n, nullptr, nullptr);
    return s;
}
std::wstring GetText(HWND h) {
    int n = GetWindowTextLengthW(h);
    if (n <= 0) return L"";
    std::wstring w(n, 0);
    GetWindowTextW(h, &w[0], n + 1);
    return w;
}

// Largest visible owner-less window of this process = the game window. Only
// non-blocking calls (no message is sent to any window).
HWND FindGameWindow() {
    struct C { HWND hwnd; long area; } c{ nullptr, 0 };
    EnumWindows([](HWND h, LPARAM lp) -> BOOL {
        if (h == g_wnd) return TRUE;
        DWORD pid = 0; GetWindowThreadProcessId(h, &pid);
        if (pid != GetCurrentProcessId() || !IsWindowVisible(h) ||
            GetWindow(h, GW_OWNER)) return TRUE;
        RECT r; if (!GetWindowRect(h, &r)) return TRUE;
        long a = (r.right - r.left) * (long)(r.bottom - r.top);
        auto* c = reinterpret_cast<C*>(lp);
        if (a > c->area) { c->area = a; c->hwnd = h; }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&c));
    return c.hwnd;
}

void CenterOverGame() {
    int x = (GetSystemMetrics(SM_CXSCREEN) - kW) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - kH) / 2;
    HWND g = FindGameWindow();
    RECT r;
    if (g && GetWindowRect(g, &r)) {
        x = r.left + ((r.right - r.left) - kW) / 2;
        y = r.top + ((r.bottom - r.top) - kH) / 2;
    }
    SetWindowPos(g_wnd, HWND_TOPMOST, x, y, kW, kH, SWP_NOACTIVATE);
}

HWND Label(HWND p, const wchar_t* t, int y, COLORREF, int x = 18, int w = 80) {
    return CreateWindowExW(0, L"STATIC", t, WS_CHILD | WS_VISIBLE,
                           x, y + 3, w, 20, p, nullptr, nullptr, nullptr);
}
HWND Edit(HWND p, int id, int y, const std::wstring& val, bool password) {
    DWORD st = WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | (password ? ES_PASSWORD : 0);
    HWND e = CreateWindowExW(0, L"EDIT", val.c_str(), st,
                             104, y, 278, 24, p, (HMENU)(INT_PTR)id, nullptr, nullptr);
    SetWindowTheme(e, L"", L"");   // classic rendering so WM_CTLCOLOREDIT applies
    return e;
}

void BuildControls(HWND h) {
    g_font    = CreateFontW(-16, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                            OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                            FF_DONTCARE, L"Segoe UI");
    g_fontHdr = CreateFontW(-20, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET,
                            OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                            FF_DONTCARE, L"Segoe UI");

    HWND hdr = CreateWindowExW(0, L"STATIC", L"Archipelago", WS_CHILD | WS_VISIBLE,
                               18, 20, 240, 30, h, nullptr, nullptr, nullptr);
    CreateWindowExW(0, L"STATIC", L"F8 to close",
                    WS_CHILD | WS_VISIBLE | SS_RIGHT, kW - 150, 26, 132, 18, h,
                    nullptr, nullptr, nullptr);
    Label(h, L"Host", 68, kFg);     g_eHost = Edit(h, IDC_HOST, 68, g_iHost, false);
    Label(h, L"Port", 102, kFg);    g_ePort = Edit(h, IDC_PORT, 102, g_iPort, false);
    Label(h, L"Slot", 136, kFg);    g_eSlot = Edit(h, IDC_SLOT, 136, g_iSlot, false);
    Label(h, L"Password", 170, kFg);g_ePass = Edit(h, IDC_PASS, 170, g_iPass, true);
    g_btn = CreateWindowExW(0, L"BUTTON", L"Connect",
                    WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                    115, 210, 170, 32, h, (HMENU)IDC_TOGGLE, nullptr, nullptr);
    g_status = Label(h, L"Disconnected", 256, kFg, 18, 364);

    // All controls get the body font, then the header overrides to the big one.
    EnumChildWindows(h, [](HWND c, LPARAM lp) -> BOOL {
        SendMessageW(c, WM_SETFONT, (WPARAM)lp, TRUE); return TRUE;
    }, (LPARAM)g_font);
    SendMessageW(hdr, WM_SETFONT, (WPARAM)g_fontHdr, TRUE);
}

// One button that reflects the connection: "Connect" when disconnected,
// "Connecting..." (greyed) while connecting, "Disconnect" once connected.
void SyncButton() {
    if (!g_btn) return;
    int st = APClient_State();
    if (st == g_lastState) return;
    g_lastState = st;
    SetWindowTextW(g_btn, st == AP_CONNECTED  ? L"Disconnect"
                        : st == AP_CONNECTING ? L"Connecting..."
                                              : L"Connect");
    EnableWindow(g_btn, st != AP_CONNECTING);   // grey out while connecting
}

LRESULT CALLBACK Proc(HWND h, UINT m, WPARAM wp, LPARAM lp) {
    switch (m) {
    case WM_CREATE:
        BuildControls(h);
        SetTimer(h, 1, 500, nullptr);
        return 0;

    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT: {
        HDC dc = (HDC)wp;
        SetTextColor(dc, kFg);
        SetBkColor(dc, m == WM_CTLCOLOREDIT ? kEdit : kBg);
        return (LRESULT)(m == WM_CTLCOLOREDIT ? g_editBr : g_bgBr);
    }

    case WM_DRAWITEM: {
        auto* d = (DRAWITEMSTRUCT*)lp;
        bool dis = (d->itemState & ODS_DISABLED) != 0;
        bool sel = (d->itemState & ODS_SELECTED) != 0;
        HBRUSH b = CreateSolidBrush(dis ? RGB(30, 32, 38)
                                        : (sel ? RGB(70, 90, 120) : kBtn));
        FillRect(d->hDC, &d->rcItem, b);
        DeleteObject(b);
        FrameRect(d->hDC, &d->rcItem, (HBRUSH)GetStockObject(BLACK_BRUSH));
        wchar_t txt[32]; GetWindowTextW(d->hwndItem, txt, 32);
        SetBkMode(d->hDC, TRANSPARENT);
        SetTextColor(d->hDC, dis ? RGB(120, 120, 128) : kFg);
        HFONT old = (HFONT)SelectObject(d->hDC, g_font);
        DrawTextW(d->hDC, txt, -1, &d->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(d->hDC, old);
        return TRUE;
    }

    case WM_COMMAND:
        if (LOWORD(wp) == IDC_TOGGLE) {
            if (APClient_State() == AP_CONNECTED) {
                APClient_Disconnect();
            } else {                              // disconnected (connecting is greyed)
                std::string host = ToA(GetText(g_eHost)), port = ToA(GetText(g_ePort));
                std::string slot = ToA(GetText(g_eSlot)), pass = ToA(GetText(g_ePass));
                AC6_ApplyConnection(host.c_str(), port.c_str(), slot.c_str(), pass.c_str());
            }
            SyncButton();                         // instant feedback (e.g. grey out)
        }
        return 0;

    case WM_TIMER:
        SyncButton();
        if (g_status) SetWindowTextW(g_status, ToW(APClient_StatusText().c_str()).c_str());
        return 0;

    case WM_TOGGLE:
        if (IsWindowVisible(h)) {
            ShowWindow(h, SW_HIDE);
        } else {
            CenterOverGame();
            ShowWindow(h, SW_SHOW);
            SetForegroundWindow(h);
            SetFocus(g_eHost);
        }
        return 0;

    case WM_DESTROY:
        KillTimer(h, 1);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, m, wp, lp);
}

void ThreadMain() {
    g_bgBr   = CreateSolidBrush(kBg);
    g_editBr = CreateSolidBrush(kEdit);

    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = Proc;
    wc.hInstance     = GetModuleHandleW(nullptr);
    wc.lpszClassName = kClass;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = g_bgBr;
    RegisterClassExW(&wc);

    g_wnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, kClass,
                            L"Archipelago", WS_POPUP,
                            200, 200, kW, kH, nullptr, nullptr, wc.hInstance, nullptr);
    if (!g_wnd) { Log("ConfigUI: CreateWindow failed (%lu)", GetLastError()); return; }
    Log("ConfigUI ready - press F8 in-game to open Archipelago settings");

    MSG msg;
    while (g_running && GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (g_font)    DeleteObject(g_font);
    if (g_fontHdr) DeleteObject(g_fontHdr);
    if (g_bgBr)    DeleteObject(g_bgBr);
    if (g_editBr)  DeleteObject(g_editBr);
    g_wnd = nullptr;
}

} // namespace

void ConfigUI_Start(const char* host, const char* port,
                    const char* slot, const char* password) {
    if (g_running.exchange(true)) return;
    g_iHost = ToW(host); g_iPort = ToW(port);
    g_iSlot = ToW(slot); g_iPass = ToW(password);
    g_thread = std::thread(ThreadMain);
}

void ConfigUI_Stop() {
    if (!g_running.exchange(false)) return;
    if (g_wnd) PostMessageW(g_wnd, WM_DESTROY, 0, 0);
    if (g_thread.joinable()) g_thread.join();
}

void ConfigUI_Toggle() {
    if (g_wnd) PostMessageW(g_wnd, WM_TOGGLE, 0, 0);
}
