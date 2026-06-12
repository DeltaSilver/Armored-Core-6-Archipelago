#include "flagwatcher.h"
#include "flagwriter.h"
#include "locations.h"
#include "apclient.h"
#include "dllmain.h"

#include <windows.h>
#include <thread>
#include <atomic>
#include <unordered_set>

// AP location IDs = AC6_BASE_ID + flagId. Must match Python BASE_LOC_ID.
#define AC6_BASE_ID 7700000

static std::atomic<bool> g_watcherRunning{ false };
static std::thread       g_watcherThread;

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
                Log("CHECK: [%u] %s", flag, g_locations[i].name);
                g_sentFlags.insert(flag);
                g_checksSent++;

                // Check if Chapter 1 First Garage Visit is True
                if (flag == 3409) {
                    SetGarageVisited();
                }

                int64_t baseId = (int64_t)AC6_BASE_ID + flag;

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

        // Goal — guard against NG+ false-fires by requiring some checks first.
        if (!g_goalReported && g_checksSent >= 10) {
            for (int i = 0; i < g_goalFlagCount; i++) {
                if (ReadEventFlag(ptr1, divisor, g_goalFlags[i])) {
                    Log("GOAL REACHED: flag %u", g_goalFlags[i]);
                    APClient_SendGoal();
                    g_goalReported = true;
                    break;
                }
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
}