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
#include "allparts.h"

#define AC6AP_VERSION "v0.1.3-Beta"

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
    bool        discover = false;       // discovery mode: log flag flips, no AP checks
    std::string discoverRanges = "";    // "a-b,c-d"; empty = sensible default
    bool        grantAllParts = false;  // enable F7 hotkey to unlock every part
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
                << "#   discover = set to 1 to log every flag that flips to\n"
                << "#              ac6ap_discovery.txt instead of sending AP checks\n"
                << "#              (used to map missions -> trigger flags). Leave 0 to play.\n"
                << "host=localhost\n"
                << "port=38281\n"
                << "slot=Player1\n"
                << "password=\n"
                << "discover=0\n"
                << "#discover_ranges=3000-3500,4000-4100,6000-6500\n"
                << "#   grant_all_parts = set to 1 to enable the F7 hotkey:\n"
                << "#              press F7 while at the garage to unlock every part\n"
                << "grant_all_parts=0\n";
        }
        return cfg;  // defaults
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
        else if (key == "discover")        cfg.discover = (val == "1" || val == "true");
        else if (key == "discover_ranges") cfg.discoverRanges = val;
        else if (key == "grant_all_parts") cfg.grantAllParts = (val == "1" || val == "true");
    }
    Log("Config loaded: host=%s port=%s slot=%s",
        cfg.host.c_str(), cfg.port.c_str(), cfg.slot.c_str());
    return cfg;
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
// I have no idea how this works.
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

    itemBuf[0] = itemId;       // itemId at offset 0, rest of page is zero

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
//  Grant queue + drain thread
// ===========================================================================

static std::mutex          g_grantMutex;
static std::queue<int>     g_grantQueue;
static std::atomic<bool>   g_grantThreadRunning{ false };

// Gate set by the flag watcher: true only while a save is loaded and safe.
static std::atomic<bool>   g_grantsSafe{ false };

void SetGrantsSafe(bool safe) {
    g_grantsSafe = safe;
}

void QueueGrant(int itemId) {
    std::lock_guard<std::mutex> lk(g_grantMutex);
    g_grantQueue.push(itemId);
}

static void GrantDrainThread(void*) {
    Log("Grant drain thread started");
    // Let the game settle after connect before granting anything.
    Sleep(1500);

    while (g_grantThreadRunning) {
        // Hold everything until a save is loaded and in a safe state.
        if (!g_grantsSafe || !g_garageVisited) {
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

// ===========================================================================
//  Grant-all-parts hotkey (F7) — fills the garage with every part.
//  Opt-in via grant_all_parts=1. Press F7 while at the garage/assembly menu
//  so the game is in a safe state to receive items. Works in discovery mode.
// ===========================================================================

static void GrantAllPartsThread(void*) {
    Log("Grant-all-parts: press F7 at the garage to unlock all %d parts.",
        g_allPartsCount);
    bool last = false;
    while (true) {
        bool pressed = (GetAsyncKeyState(VK_F7) & 0x8000) != 0;
        if (pressed && !last) {
            if (!g_AddItem) {
                Log("F7: AddItem not ready yet — load into the garage first.");
            } else {
                Log("F7 pressed — granting all %d parts...", g_allPartsCount);
                for (int i = 0; i < g_allPartsCount; i++) {
                    GrantItem(g_allParts[i], 1);
                    Sleep(40);   // space them out so the game keeps up
                }
                Log("F7: finished granting all parts. Back out and re-enter the "
                    "assembly menu if they don't show immediately.");
            }
        }
        last = pressed;
        Sleep(50);
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

    // Locate the AddItem function within game code
    FindAddItemFunction();

    // F6 Bug testing
    //#define TEST_ITEM_ID 65000000

    //std::thread([]() {
    //    bool lastState = false;
    //    while (true) {
    //        bool pressed = (GetAsyncKeyState(VK_F6) & 0x8000) != 0;
    //        if (pressed && !lastState) {
    //            Log("F6 pressed — granting test item 0x%X", TEST_ITEM_ID);
    //            GrantItem(TEST_ITEM_ID, 1);
    //        }
    //        lastState = pressed;
    //        Sleep(50);
    //    }
    //    }).detach();

    // Start the grant drain thread.
    g_grantThreadRunning = true;
    _beginthread(GrantDrainThread, 0, nullptr);

    // Load connection settings from ac6ap.cfg, then connect.
    AC6Config cfg = LoadConfig(GetDllDir() + "ac6ap.cfg");

    // Optional: F7 hotkey to unlock the whole garage (works in any mode).
    if (cfg.grantAllParts) {
        _beginthread(GrantAllPartsThread, 0, nullptr);
    }

    // Discovery mode: don't touch Archipelago at all — just log flag flips so we
    // can map missions -> trigger flags in one playthrough.
    if (cfg.discover) {
        Log("discover=1 — running flag discovery (no AP connection / no checks).");
        FlagWatcher_StartDiscovery(eventFlagMan, cfg.discoverRanges.c_str());
        Log("Setup complete. Discovery logging to ac6ap_discovery.txt.");
        while (true) Sleep(1000);
        return;
    }

    std::string scheme = (cfg.host.find("archipelago.gg") != std::string::npos)
        ? "wss://" : "ws://";
    std::string uri = scheme + cfg.host + ":" + cfg.port;
    APClient_Connect(uri.c_str(), cfg.slot.c_str(), cfg.password.c_str());

    // Start watching flags.
    FlagWatcher_Start(eventFlagMan);

    Log("Setup complete. Watching flags and granting received items.");

    // Keep the thread alive.
    while (true) {
        Sleep(1000);
    }
}

// ===========================================================================
//  DLL entry
// ===========================================================================

static void OnLoad() {
    std::string logPath = GetDllDir() + "ac6ap_log.txt";
    g_logFile = _fsopen(logPath.c_str(), "w", _SH_DENYWR);
    Log("DLL loaded successfully (version %s)", AC6AP_VERSION);
    _beginthread(MainThread, 0, nullptr);
}

static void OnUnload() {
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