#include "flagwatcher.h"
#include "flagwriter.h"
#include "locations.h"
#include "apclient.h"
#include "dllmain.h"
#include "memory.h"

#include <windows.h>
#include <thread>
#include <atomic>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <string>
#include <cstdio>
#include <cstdint>
#include <cstdarg>

#define AC6AP_LOG_TAG "FLAG_READ"

// AP location IDs = AC6_BASE_ID + flagId. Must match Python BASE_LOC_ID.
#define AC6_BASE_ID 7700000

static std::atomic<bool> g_watcherRunning{ false };
static std::thread       g_watcherThread;
static std::atomic<bool> g_discoverRunning{ false };  // discovery mode (defined section below)

// OUTER CSEventFlagMan address.
static uintptr_t g_eventFlagMan = 0;

// Flags it has already reported, so it never send a check twice this session.
static std::unordered_set<uint32_t> g_sentFlags;
static int  g_checksSent = 0;
static bool g_goalReported = false;

static void WatcherLoop() {
    Log("FlagWatcher started, watching %d locations", g_locationCount);

    while (g_watcherRunning) {
        // Re-read the live save pointer + divisor every poll. Both go
        // invalid on the title screen / during load.
        uintptr_t ptr1 = *(uintptr_t*)g_eventFlagMan;
        int32_t   divisor = 0;
        bool      safe = false;
        if (ptr1) {
            divisor = *(int32_t*)(ptr1 + 0x1C);
            safe = (divisor != 0);
        }

        // Drive the grant gate: only allow grants while a save is live.
        SetGrantsSafe(safe);

        if (!safe) {
            Sleep(500);
            continue;
        }

        // Location checks
        for (int i = 0; i < g_locationCount; i++) {
            uint32_t flag = g_locations[i].flagId;
            if (g_sentFlags.count(flag)) continue;

            if (ReadEventFlag(ptr1, divisor, flag)) {
                int cycle = APClient_GetCycle();
                Log("CHECK: [%u] %s (cycle %d)", flag, g_locations[i].name, cycle);
                g_sentFlags.insert(flag);
                g_checksSent++;

                // Check if Chapter 1 First Garage Visit is True
                if (flag == 3409) {
                    SetGarageVisited();
                }

                // Story/mission flags (3000-3999) reset each NG cycle, so their
                // location IDs are offset by cycle to stay distinct across
                // NG / NG+ / NG++. Persistent flags (arena, ranks, key missions,
                // archives) are never offset.
                int64_t cycleOff = AC6_IsCycledFlag(flag)
                    ? (int64_t)cycle * AC6_CYCLE_OFFSET : 0;
                int64_t baseId = (int64_t)AC6_BASE_ID + cycleOff + flag;

                // Archive flags (4000-4049) are a separate experimental
                bool isArchive = (flag >= 4000 && flag <= 4049);

                if (isArchive) {
                    APClient_SendCheck(baseId);
                }
                else {
                    // Core checks: always send every multiplier band. The
                    // server keeps only the bands the seed generated (per the
                    // player's multiplier) and ignores the rest, so the DLL
                    // never needs to know the multiplier value.
                    for (int band = 0; band < AC6_MAX_MULTIPLIER; band++) {
                        APClient_SendCheck(
                            baseId + (int64_t)band * AC6_MULTIPLIER_OFFSET);
                    }
                }
            }
        }

        // NG cycle reset detection. When a new game cycle (NG+/NG++) begins the
        // game wipes all story progress flags at once. If many already-sent
        // story flags read clear in a single poll, treat it as a cycle reset:
        // advance the cycle and forget the story flags so they re-send as the
        // next cycle's (distinct) checks. Persistent flags are left alone.
        {
            int clearedStory = 0;
            for (uint32_t f : g_sentFlags)
                if (AC6_IsCycledFlag(f) && !ReadEventFlag(ptr1, divisor, f))
                    clearedStory++;

            if (clearedStory >= 8) {
                Log("NG cycle reset detected (%d story flags cleared at once).",
                    clearedStory);
                APClient_AdvanceCycle();
                for (auto it = g_sentFlags.begin(); it != g_sentFlags.end(); ) {
                    if (AC6_IsCycledFlag(*it)) it = g_sentFlags.erase(it);
                    else ++it;
                }
            }
        }

        // Goal — guard against NG+ false-fires by requiring some checks first.
        // Ending flags (6000/6001/6002) persist across cycles, so the goal is
        // reached once enough of them are set: single=1 (any ending),
        // ng_plus_*=2, full_*=3 (handled by APClient_GetRequiredEndings).
        if (!g_goalReported && g_checksSent >= 10) {
            int required = APClient_GetRequiredEndings();
            int setCount = 0;
            for (int i = 0; i < g_goalFlagCount; i++)
                if (ReadEventFlag(ptr1, divisor, g_goalFlags[i])) setCount++;
            if (setCount >= required) {
                Log("GOAL REACHED (%d/%d endings, run mode %d)",
                    setCount, required, APClient_GetRunMode());
                APClient_SendGoal();
                g_goalReported = true;
            }
        }

        Sleep(500);
    }
}

void FlagWatcher_Start(uintptr_t eventFlagMan) {
    if (g_watcherRunning) return;
    g_eventFlagMan = eventFlagMan;
    g_watcherRunning = true;
    g_watcherThread = std::thread(WatcherLoop);
    g_watcherThread.detach();
}

void FlagWatcher_Stop() {
    g_watcherRunning = false;
    g_discoverRunning = false;
}

// ===========================================================================
//  Discovery mode
//
//  Scans a set of flag ranges every poll and logs each flag that changes
//  state to a dedicated file. One instrumented playthrough then shows exactly
//  which flags a mission flips (correlate by timestamp). Does NOT send AP
//  checks — this mode is purely for mapping missions -> flags.
// ===========================================================================

static std::thread       g_discoverThread;
static std::vector<std::pair<uint32_t, uint32_t>> g_discoverRanges;
static std::string       g_discoverRangesDesc;
static FILE*             g_discFile = nullptr;

static void DiscLog(const char* format, ...) {
    if (!g_discFile) return;
    SYSTEMTIME t; GetLocalTime(&t);
    fprintf(g_discFile, "%02d:%02d:%02d  ", t.wHour, t.wMinute, t.wSecond);
    va_list args; va_start(args, format);
    vfprintf(g_discFile, format, args);
    va_end(args);
    fputc('\n', g_discFile);
    fflush(g_discFile);
}

// Parse "a-b,c-d,e" into inclusive ranges. A bare "e" becomes [e,e].
static void ParseRanges(const std::string& spec) {
    g_discoverRanges.clear();
    size_t i = 0;
    while (i < spec.size()) {
        size_t comma = spec.find(',', i);
        std::string tok = spec.substr(i, comma == std::string::npos ? std::string::npos : comma - i);
        // trim spaces
        size_t a = tok.find_first_not_of(" \t");
        size_t b = tok.find_last_not_of(" \t");
        if (a != std::string::npos) {
            tok = tok.substr(a, b - a + 1);
            size_t dash = tok.find('-');
            uint32_t lo, hi;
            if (dash == std::string::npos) {
                lo = hi = (uint32_t)strtoul(tok.c_str(), nullptr, 10);
            } else {
                lo = (uint32_t)strtoul(tok.substr(0, dash).c_str(), nullptr, 10);
                hi = (uint32_t)strtoul(tok.substr(dash + 1).c_str(), nullptr, 10);
            }
            if (hi >= lo) g_discoverRanges.push_back({ lo, hi });
        }
        if (comma == std::string::npos) break;
        i = comma + 1;
    }
}

static void DiscoveryLoop() {
    std::string path = GetDllDir() + "ac6ap_discovery.txt";
    // Append mode so a game restart continues the same log instead of wiping
    // it. A restart re-dumps the current save's set flags as a baseline, then
    // resumes logging changes.
    g_discFile = _fsopen(path.c_str(), "a", _SH_DENYWR);
    Log("Discovery mode active -> %s", path.c_str());

    DiscLog("=== AC6AP flag discovery ===");
    DiscLog("ranges: %s", g_discoverRangesDesc.c_str());
    DiscLog("Waiting for a live save... play a mission and watch the SET lines.");

    std::unordered_map<uint32_t, bool> state;
    bool baselined = false;

    while (g_discoverRunning) {
        uintptr_t ptr1 = *(uintptr_t*)g_eventFlagMan;
        int32_t   divisor = 0;
        bool      safe = false;
        if (ptr1) { divisor = *(int32_t*)(ptr1 + 0x1C); safe = (divisor != 0); }

        if (!safe) { Sleep(500); continue; }

        if (!baselined) {
            int setCount = 0;
            for (auto& r : g_discoverRanges)
                for (uint32_t f = r.first; f <= r.second; ++f) {
                    bool v = ReadEventFlag(ptr1, divisor, f);
                    state[f] = v;
                    if (v) { DiscLog("  initial: flag %u is SET", f); setCount++; }
                }
            DiscLog("--- baseline complete: %d flags already set. Logging changes below. ---", setCount);
            baselined = true;
        } else {
            for (auto& r : g_discoverRanges)
                for (uint32_t f = r.first; f <= r.second; ++f) {
                    bool v = ReadEventFlag(ptr1, divisor, f);
                    bool& prev = state[f];   // inserts false if new
                    if (prev != v) {
                        DiscLog("flag %u -> %s", f, v ? "SET" : "clear");
                        prev = v;
                    }
                }
        }
        Sleep(500);
    }

    if (g_discFile) { DiscLog("=== discovery stopped ==="); fclose(g_discFile); g_discFile = nullptr; }
}

void FlagWatcher_StartDiscovery(uintptr_t eventFlagMan, const char* ranges) {
    if (g_discoverRunning) return;
    g_eventFlagMan = eventFlagMan;
    g_discoverRangesDesc = (ranges && *ranges) ? ranges
        : "3000-3500,4000-4100,6000-6500";
    ParseRanges(g_discoverRangesDesc);
    g_discoverRunning = true;
    g_discoverThread = std::thread(DiscoveryLoop);
    g_discoverThread.detach();
}