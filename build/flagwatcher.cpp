#include "flagwatcher.h"
#include "flagwriter.h"
#include "locations.h"
#include "apclient.h"
#include "dllmain.h"
#include "memory.h"
#include "shopslots.h"
#include "overlay.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <thread>
#include <atomic>
#include <unordered_set>
#include <string>
#include <cstdint>

#define AC6AP_LOG_TAG "FLAG_READ"

static const uint32_t SHOP_GATE_FLAGS[4] = { 3409, 3453, 3412, 3464 };
static bool g_shopChUnlocked[4] = { false, false, false, false };

// AP location IDs = AC6_BASE_ID + flagId. Must match Python BASE_LOC_ID.
#define AC6_BASE_ID 7700000

static std::atomic<bool> g_watcherRunning{ false };
static std::thread       g_watcherThread;

static uintptr_t g_eventFlagMan = 0;

// Flags it has already reported, so it never send a check twice this session.
static std::unordered_set<uint32_t> g_sentFlags;
static int  g_checksSent = 0;
static bool g_goalReported = false;

static void WatcherLoop() {
    Log("FlagWatcher started, watching %d locations", g_locationCount);

    while (g_watcherRunning) {
        uintptr_t ptr1 = *(uintptr_t*)g_eventFlagMan;
        int32_t   divisor = 0;
        bool      safe = false;
        if (ptr1) {
            divisor = *(int32_t*)(ptr1 + 0x1C);
            safe = (divisor != 0);
        }

        SetGrantsSafe(safe);

        if (!safe) {
            Sleep(500);
            continue;
        }

        if (ReadEventFlag(ptr1, divisor, 3409)) SetGarageVisited();

        // Location checks
        for (int i = 0; i < g_locationCount; i++) {
            uint32_t flag = g_locations[i].flagId;
            if (g_sentFlags.count(flag)) continue;

            if (ReadEventFlag(ptr1, divisor, flag)) {
                int cycle = APClient_GetCycle();
                Log("CHECK: [%u] %s (cycle %d)", flag, g_locations[i].name, cycle);
                g_sentFlags.insert(flag);
                g_checksSent++;

                // Story/mission flags (3000-3999) reset each NG cycle, so their
                // location IDs are offset by cycle to stay distinct across
                // NG / NG+ / NG++. Persistent flags (arena, ranks, key missions,
                // archives) are never offset.
                int64_t cycleOff = AC6_IsCycledFlag(flag)
                    ? (int64_t)cycle * AC6_CYCLE_OFFSET : 0;
                int64_t baseId = (int64_t)AC6_BASE_ID + cycleOff + flag;

                // Archive flags (4000-4049) are a separate experimental
                bool isArchive = (flag >= 4000 && flag <= 4049);

                Log("flag %u -> \"%s\" [cycle %d] => locId %lld%s",
                    flag, g_locations[i].name, cycle, (long long)baseId,
                    isArchive ? "" : " (+reward bands)");
                Overlay_Message(OVL_SENT, "Check  %s", g_locations[i].name);

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

        // ── Shop chapter batch unlocking ──────────────────────────────────
        {
            int bpc = APClient_GetBatchesPerCh();
            if (bpc > 0) {
                for (int ch = 0; ch < AC6_SHOP_CHAPTERS; ch++) {
                    if (!g_shopChUnlocked[ch] &&
                        ReadEventFlag(ptr1, divisor, SHOP_GATE_FLAGS[ch])) {
                        g_shopChUnlocked[ch] = true;
                        for (int b = 0; b < bpc && b < AC6_SHOP_BATCHES_PER_CH; b++) {
                            uint32_t batchFlag = AC6_SHOP_BATCH_FLAG_BASE
                                + ch * AC6_SHOP_BATCHES_PER_CH + b;
                            WriteEventFlag(ptr1, divisor, batchFlag, true);
                            Log("Shop Ch%d Batch%d unlocked (wrote flag %u)",
                                ch + 1, b + 1, batchFlag);
                        }
                    }
                }
            }
        }

        // ── Shop purchase checks ───────────────────────────────────────────
        for (const auto& slot : g_shopSlots) {
            if (!g_shopChUnlocked[slot.chapter - 1]) continue;
            if (g_sentFlags.count(slot.flag_id)) continue;

            if (ReadEventFlag(ptr1, divisor, slot.flag_id)) {
                Log("SHOP CHECK: [Ch%d slot %d] %s",
                    slot.chapter, slot.slot, slot.display_name.c_str());
                g_sentFlags.insert(slot.flag_id);
                g_checksSent++;
                Log("shop flag %u -> \"%s\" [Ch%d] => locId %lld",
                    slot.flag_id, slot.display_name.c_str(), slot.chapter,
                    (long long)slot.ap_loc_id);
                Overlay_Message(OVL_SENT, "Shop  %s", slot.display_name.c_str());
                APClient_SendCheck(slot.ap_loc_id);
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

        // ── Goal ──────────────────────────────────────────────────────────
        if (!g_goalReported && g_checksSent >= 10) {
            int required = APClient_GetRequiredEndings();
            int setCount = 0;
            for (int i = 0; i < g_goalFlagCount; i++)
                if (ReadEventFlag(ptr1, divisor, g_goalFlags[i])) setCount++;
            if (setCount >= required) {
                Log("GOAL REACHED (%d/%d endings, run mode %d)",
                    setCount, required, APClient_GetRunMode());
                Overlay_Message(OVL_INFO, "Goal complete!  (%d/%d endings)",
                    setCount, required);
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
}