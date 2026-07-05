#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <share.h>
#include <process.h>
#include <thread>
#include <mutex>
#include <queue>
#include <atomic>
#include <string>
#include <fstream>

#define AC6AP_LOG_TAG "DLL"
#include "dllmain.h"
#include "memory.h"
#include "flagwriter.h"
#include "flagwatcher.h"
#include "apclient.h"
#include "partnames.h"
#include "overlay.h"
#include "configui.h"

#define AC6AP_VERSION "v0.2.0-Beta"

static FILE* g_logFile = nullptr;

void LogTagged(const char* tag, const char* format, ...) {
    va_list args;
    va_start(args, format);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (g_logFile) {
        SYSTEMTIME t;
        GetLocalTime(&t);
        fprintf(g_logFile, "%02d:%02d:%02d [%s] %s\n",
            t.wHour, t.wMinute, t.wSecond, tag, buffer);
        fflush(g_logFile);
    }
}

// ===========================================================================
//  Connection config (ac6ap.cfg, next to the DLL)
// ===========================================================================

struct AC6Config {
    std::string host = "localhost";
    std::string port = "38281";
    std::string slot = "Player1";
    std::string password = "";
    float       uiScale = 1.0f;
};

static std::string Trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    size_t b = s.find_last_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    return s.substr(a, b - a + 1);
}

static AC6Config LoadConfig(const std::string& path) {
    AC6Config cfg;

    std::ifstream f(path);
    if (!f.is_open()) {
        Log("Config not found at %s - writing default", path.c_str());
        std::ofstream out(path);
        if (out.is_open()) {
            out << "# Armored Core VI Archipelago - connection settings\n"
                << "# Edit these to match your room, save, then relaunch.\n"
                << "#   host     = server address (e.g. archipelago.gg or localhost)\n"
                << "#   port     = room port number\n"
                << "#   slot     = your slot name, exactly as in your YAML\n"
                << "#   password = room password, or leave blank\n"
                << "host=localhost\n"
                << "port=38281\n"
                << "slot=Player1\n"
                << "password=\n"
                << "#   ui_scale = size of the overlay + F8 settings window.\n"
                << "#              1.0 = default. Try 1.5, 2.0, 3.0 for bigger.\n"
                << "ui_scale=1.0\n";
        }
        return cfg;
    }

    std::string line;
    while (std::getline(f, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == '#') continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = Trim(line.substr(0, eq));
        std::string val = Trim(line.substr(eq + 1));
        if (key == "host")     cfg.host = val;
        else if (key == "port")     cfg.port = val;
        else if (key == "slot")     cfg.slot = val;
        else if (key == "password") cfg.password = val;
        else if (key == "ui_scale")        cfg.uiScale = (float)atof(val.c_str());
    }
    Log("Config loaded: host=%s port=%s slot=%s",
        cfg.host.c_str(), cfg.port.c_str(), cfg.slot.c_str());
    return cfg;
}

static AC6Config  g_cfg;
static std::string g_cfgPath;

static std::string BuildUri(const std::string& host, const std::string& port) {
    std::string scheme = (host.find("archipelago.gg") != std::string::npos)
        ? "wss://" : "ws://";
    return scheme + host + ":" + port;
}

static void SaveConfig() {
    if (g_cfgPath.empty()) return;
    std::ofstream out(g_cfgPath);
    if (!out.is_open()) { Log("SaveConfig: could not write %s", g_cfgPath.c_str()); return; }
    out << "# Armored Core VI Archipelago - connection settings.\n"
        << "# Edited in-game via the F8 settings window (or by hand).\n"
        << "host=" << g_cfg.host << "\n"
        << "port=" << g_cfg.port << "\n"
        << "slot=" << g_cfg.slot << "\n"
        << "password=" << g_cfg.password << "\n\n"
        << "# Display\n"
        << "ui_scale=" << g_cfg.uiScale << "\n";
    Log("Config saved to %s", g_cfgPath.c_str());
}

void AC6_ApplyConnection(const char* host, const char* port,
    const char* slot, const char* password) {
    g_cfg.host = host ? host : "";
    g_cfg.port = port ? port : "";
    g_cfg.slot = slot ? slot : "";
    g_cfg.password = password ? password : "";
    SaveConfig();
    std::string uri = BuildUri(g_cfg.host, g_cfg.port);
    Log("Applying connection: %s as %s", uri.c_str(), g_cfg.slot.c_str());
    APClient_Reconnect(uri.c_str(), g_cfg.slot.c_str(), g_cfg.password.c_str());
}

// ===========================================================================
//  Flag to check if the save has been loaded
// ===========================================================================

static std::atomic<bool> g_garageVisited{ false };

void SetGarageVisited() {
    if (!g_garageVisited) {
        g_garageVisited = true;
        Log("Garage visited — item grants now active");
    }
}

// ===========================================================================
//  Item granting - AC6's internal AddItem function
//
//  Found by AOB scanning for the CALL site documented in the TGA CT table.
//  The instruction stream around the match is:
//     48 89 45 58   mov  [rbp+58], rax
//     8B 45 48      mov  eax, [rbp+48]
//     89 45 50      mov  [rbp+50], eax
//     8B D7         mov  edx, edi        ; quantity  -> rdx (arg2)
//     48 8D 4D 50   lea  rcx, [rbp+50]   ; &itemId   -> rcx (arg1)
//     E8 <rel32>    call AddItem         ; <-- offset 16 in the match
//  So the function = (matchAddr + 21) + rel32.
//
//  Signature: void AddItem(int* itemIdPtr, int quantity)
//    rcx = pointer to the item ID, rdx = quantity.
// 
// ===========================================================================

typedef void(*AddItemFunc)(int* itemIdPtr, int quantity);
static AddItemFunc g_AddItem = nullptr;

void FindAddItemFunction() {
    uintptr_t addr = AOBScan("?? 89 ?? ?? 8B ?? ?? 89 ?? ?? 8B D7 ?? 8D");
    if (!addr) {
        Log("AddItem pattern not found");
        return;
    }
    Log("AddItem pattern found at 0x%llX", addr);

    // CALL (E8) is at offset 16; rel32 is the 4 bytes at offset 17.
    int32_t rel = *(int32_t*)(addr + 17);
    uintptr_t funcAddr = addr + 21 + rel;
    g_AddItem = (AddItemFunc)funcAddr;
    Log("AddItem function resolved to 0x%llX", funcAddr);

    // Sanity log - expect a real prologue, e.g. 40 57 48 83 EC 40 ...
    uint8_t* b = (uint8_t*)funcAddr;
    Log("First bytes: %02X %02X %02X %02X %02X %02X %02X %02X",
        b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7]);
}

void GrantItem(int itemId, int quantity) {
    if (!g_AddItem) {
        Log("GrantItem: AddItem not available");
        return;
    }

    // Allocate a zeroed page so any extra
    // struct fields the function reads past the ID come back as 0.
    int* itemBuf = (int*)VirtualAlloc(
        nullptr, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!itemBuf) {
        Log("GrantItem: alloc failed");
        return;
    }

    itemBuf[0] = itemId;

    __try {
        g_AddItem(itemBuf, quantity);
        const char* name = GetPartName(itemId);
        if (name)
            Log("Granted: %s", name);
        else
            Log("Granted item 0x%X x%d", itemId, quantity);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        Log("GrantItem: EXCEPTION granting item 0x%X (code 0x%lX)",
            itemId, GetExceptionCode());
    }

    VirtualFree(itemBuf, 0, MEM_RELEASE);
}

// ===========================================================================
//  COAM (credits) granting - AC6's internal add-currency function
//
//  Ported from the TGA CT "AddSoul" script. Unlike AddItem, this AOB pattern
//  matches the add-currency FUNCTION'S OWN BODY (a LEA-based add-and-store),
//  not a CALL site — so the scanned address is directly callable, no
//  relative-CALL resolution needed.
//
//  Signature: void AddCOAM(void* playerGameData, int32_t amount)
//    rcx = PlayerGameData pointer (GameDataMan + 0x8), rdx = amount to add.
// ===========================================================================

typedef void(*AddCoamFunc)(void* playerGameData, int32_t amount);
static AddCoamFunc g_AddCoam     = nullptr;
static uintptr_t   g_gameDataMan = 0;

void FindAddCoamFunction() {
    // GameDataMan: RIP-relative mov, same resolution as EventFlagMan.
    // AOB from TGA CT: ?? 8B 05 ?? ?? ?? ?? ?? 85 C0 ?? ?? ?? 8B 40 ?? C3
    // The pointer operand is at offset 3, instruction length 7.
    uintptr_t gdmResult = AOBScan("?? 8B 05 ?? ?? ?? ?? ?? 85 C0 ?? ?? ?? 8B 40 ?? C3");
    if (!gdmResult) {
        Log("GameDataMan pattern not found - COAM grants disabled");
        return;
    }
    g_gameDataMan = ResolveRelativePointer(gdmResult, 3, 7);
    Log("GameDataMan resolved to 0x%llX", g_gameDataMan);

    uintptr_t addr = AOBScan("?? 8B ?? ?? ?? 8D 0C 10 ?? 89 4C ?? 10");
    if (!addr) {
        Log("AddCOAM pattern not found");
        return;
    }
    g_AddCoam = (AddCoamFunc)addr;
    Log("AddCOAM function resolved to 0x%llX", addr);

    uint8_t* b = (uint8_t*)addr;
    Log("AddCOAM first bytes: %02X %02X %02X %02X %02X %02X %02X %02X",
        b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7]);
}

void GrantCOAM(int32_t amount) {
    if (!g_AddCoam || !g_gameDataMan) {
        Log("GrantCOAM: not available (AddCoam=%p GameDataMan=0x%llX)",
            (void*)g_AddCoam, (unsigned long long)g_gameDataMan);
        return;
    }

    uintptr_t gdm = *(uintptr_t*)g_gameDataMan;
    if (!gdm) { Log("GrantCOAM: GameDataMan not loaded yet"); return; }

    void* playerGameData = *(void**)(gdm + 0x8);
    if (!playerGameData) { Log("GrantCOAM: PlayerGameData null"); return; }

    __try {
        g_AddCoam(playerGameData, amount);
        Log("Granted: %d COAM", amount);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        Log("GrantCOAM: EXCEPTION granting %d COAM (code 0x%lX)",
            amount, GetExceptionCode());
    }
}

// ===========================================================================
//  Grant queue + drain thread
// ===========================================================================

static std::mutex          g_grantMutex;
static std::queue<int>     g_grantQueue;
static std::mutex          g_coamMutex;
static std::queue<int32_t> g_coamQueue;
static std::atomic<bool>   g_grantThreadRunning{ false };

static std::atomic<bool>   g_grantsSafe{ false };

void SetGrantsSafe(bool safe) {
    g_grantsSafe = safe;
}

void QueueGrant(int itemId) {
    std::lock_guard<std::mutex> lk(g_grantMutex);
    g_grantQueue.push(itemId);
}

void QueueGrantCOAM(int32_t amount) {
    std::lock_guard<std::mutex> lk(g_coamMutex);
    g_coamQueue.push(amount);
}

static void GrantDrainThread(void*) {
    Log("Grant drain thread started");
    Sleep(1500);

    while (g_grantThreadRunning) {
        if (!g_grantsSafe || !g_garageVisited) {
            Sleep(250);
            continue;
        }

        int32_t coamAmt = 0;
        bool haveCoam = false;
        {
            std::lock_guard<std::mutex> lk(g_coamMutex);
            if (!g_coamQueue.empty()) {
                coamAmt = g_coamQueue.front();
                g_coamQueue.pop();
                haveCoam = true;
            }
        }
        if (haveCoam) {
            GrantCOAM(coamAmt);
            Sleep(250);
            continue;
        }

        int itemId = -1;
        bool have = false;
        {
            std::lock_guard<std::mutex> lk(g_grantMutex);
            if (!g_grantQueue.empty()) {
                itemId = g_grantQueue.front();
                g_grantQueue.pop();
                have = true;
            }
        }

        if (have) {
            GrantItem(itemId, 1);
            Sleep(250);   // space grants out - one at a time - maybe too long?
        }
        else {
            Sleep(100);
        }
    }
}

// F8 toggles the in-game Archipelago settings window (connect / change room).
static void SettingsHotkeyThread(void*) {
    bool last = false;
    while (true) {
        bool pressed = (GetAsyncKeyState(VK_F8) & 0x8000) != 0;
        if (pressed && !last) ConfigUI_Toggle();
        last = pressed;
        Sleep(50);
    }
}

static HWND FindGameMainWindow() {
    struct C { HWND hwnd; long area; } ctx{ nullptr, 0 };
    EnumWindows([](HWND h, LPARAM lp) -> BOOL {
        DWORD pid = 0; GetWindowThreadProcessId(h, &pid);
        if (pid != GetCurrentProcessId() || GetWindow(h, GW_OWNER)) return TRUE;
        wchar_t cls[64]; GetClassNameW(h, cls, 64);
        if (lstrcmpiW(cls, L"AC6AP_Settings") == 0 ||
            lstrcmpiW(cls, L"AC6AP_Overlay") == 0) return TRUE;
        RECT r; if (!GetWindowRect(h, &r)) return TRUE;
        long a = (long)(r.right - r.left) * (r.bottom - r.top);
        auto* c = reinterpret_cast<C*>(lp);
        if (a > c->area) { c->area = a; c->hwnd = h; }
        return TRUE;
        }, reinterpret_cast<LPARAM>(&ctx));
    return ctx.hwnd;
}

static void WatchdogThread(void*) {
    while (!FindGameMainWindow()) Sleep(1000);   // wait for the game window
    Log("Watchdog: tracking game window for shutdown.");
    int misses = 0;
    while (true) {
        Sleep(1000);
        if (FindGameMainWindow()) misses = 0;
        else if (++misses >= 4) {
            Log("Game window gone - exiting process.");
            ExitProcess(0);
        }
    }
}

// ===========================================================================
//  Main worker thread
// ===========================================================================

static void MainThread(void*) {
    Log("Waiting for game to initialise...");

    // Find CSEventFlagMan.
    uintptr_t result = AOBScan("48 8B 35 ? ? ? ? 83 F8 FF 0F 44 C1");
    if (!result) {
        Log("EventFlagMan pattern not found, aborting");
        return;
    }
    uintptr_t eventFlagMan = ResolveRelativePointer(result, 3, 7);

    // Wait for a save to load (divisor only becomes non-zero then).
    int32_t  divisor = 0;
    uintptr_t ptr1 = 0;
    while (divisor == 0) {
        Sleep(1000);
        ptr1 = *(uintptr_t*)eventFlagMan;
        if (!ptr1) { Log("ptr1 null, waiting..."); continue; }
        divisor = *(int32_t*)(ptr1 + 0x1C);
        Log("Waiting... divisor = %d", divisor);
    }
    Log("Game initialised! Divisor: %d", divisor);

    FindAddItemFunction();
    FindAddCoamFunction();

    // F6 — COAM grant test. Press F6 in-game to grant 50000 COAM.
    // Remove once GameDataMan is confirmed working.
    std::thread([]() {
        bool last = false;
        while (true) {
            bool pressed = (GetAsyncKeyState(VK_F6) & 0x8000) != 0;
            if (pressed && !last) {
                Log("F6 pressed — granting test COAM (50000)");
                GrantCOAM(50000);
            }
            last = pressed;
            Sleep(50);
        }
    }).detach();

    g_grantThreadRunning = true;
    _beginthread(GrantDrainThread, 0, nullptr);

    _beginthread(WatchdogThread, 0, nullptr);

    g_cfgPath = GetDllDir() + "ac6ap.cfg";
    AC6Config cfg = LoadConfig(g_cfgPath);
    g_cfg = cfg;

    std::string uri = BuildUri(cfg.host, cfg.port);
    APClient_Connect(uri.c_str(), cfg.slot.c_str(), cfg.password.c_str());

    FlagWatcher_Start(eventFlagMan);

    Overlay_SetScale(cfg.uiScale);
    Overlay_Start();
    Overlay_Message(OVL_INFO, "Armored Core VI Archipelago connected");

    ConfigUI_SetScale(cfg.uiScale);
    ConfigUI_Start(cfg.host.c_str(), cfg.port.c_str(),
        cfg.slot.c_str(), cfg.password.c_str());
    _beginthread(SettingsHotkeyThread, 0, nullptr);

    Log("Setup complete. Watching flags and granting received items.");

    // Keep the thread alive.
    while (true) {
        Sleep(1000);
    }
}

// ===========================================================================
//  DLL entry
// ===========================================================================

static void RotateLog(const std::string& dir, const std::string& logPath) {
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExA(logPath.c_str(), GetFileExInfoStandard, &fad))
        return;

    std::string logsDir = dir + "APLogs";
    CreateDirectoryA(logsDir.c_str(), nullptr);

    SYSTEMTIME st;
    FILETIME   lt;
    if (!(FileTimeToLocalFileTime(&fad.ftLastWriteTime, &lt) &&
        FileTimeToSystemTime(&lt, &st)))
        GetLocalTime(&st);

    char name[80];
    snprintf(name, sizeof(name), "ac6ap_%04d-%02d-%02d_%02d-%02d-%02d.txt",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    std::string dest = logsDir + "\\" + name;

    if (GetFileAttributesA(dest.c_str()) != INVALID_FILE_ATTRIBUTES) {
        for (int i = 2; i < 1000; i++) {
            char alt[96];
            snprintf(alt, sizeof(alt), "ac6ap_%04d-%02d-%02d_%02d-%02d-%02d_%d.txt",
                st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, i);
            std::string cand = logsDir + "\\" + alt;
            if (GetFileAttributesA(cand.c_str()) == INVALID_FILE_ATTRIBUTES) { dest = cand; break; }
        }
    }

    MoveFileA(logPath.c_str(), dest.c_str());
}

static void OnLoad() {
    std::string dir = GetDllDir();
    std::string logPath = dir + "ac6ap_log.txt";
    RotateLog(dir, logPath);
    g_logFile = _fsopen(logPath.c_str(), "w", _SH_DENYWR);
    Log("DLL loaded successfully (version %s)", AC6AP_VERSION);
    _beginthread(MainThread, 0, nullptr);
}

static void OnUnload() {
    ConfigUI_Stop();
    Overlay_Stop();
    FlagWatcher_Stop();
    APClient_Disconnect();
    g_grantThreadRunning = false;
    Log("DLL unloading");
    if (g_logFile) {
        fclose(g_logFile);
        g_logFile = nullptr;
    }
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinstDLL);
        OnLoad();
    }
    else if (fdwReason == DLL_PROCESS_DETACH) {
        OnUnload();
    }
    return TRUE;
}