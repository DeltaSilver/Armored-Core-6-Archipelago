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

static const uint32_t SHOP_GATE_FLAGS[4] = { 3409, 3410, 3420, 3430 };
static bool g_shopChUnlocked[4] = { false, false, false, false };

// AP location IDs = AC6_BASE_ID + flagId. Must match Python BASE_LOC_ID.
#define AC6_BASE_ID 7700000

static std::atomic<bool> g_watcherRunning{ false };
static std::thread       g_watcherThread;

static std::atomic<bool> g_deathLinkPending{ false };
static std::atomic<bool> g_suppressNextDeathSend{ false };

void FlagWatcher_TriggerDeathLink() {
    Log("DeathLink: trigger queued");
    g_deathLinkPending = true;
}

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

        // DeathLink diagnostics: log flag 9901/9903 changes.
        {
            static bool last9901 = false, last9903 = false, first = true;
            bool cur9901 = ReadEventFlag(ptr1, divisor, 9901);
            bool cur9903 = ReadEventFlag(ptr1, divisor, 9903);
            if (first || cur9901 != last9901)
                Log("DeathLink diag: flag 9901 = %d", cur9901 ? 1 : 0);
            if (first || cur9903 != last9903)
                Log("DeathLink diag: flag 9903 = %d", cur9903 ? 1 : 0);
            last9901 = cur9901; last9903 = cur9903; first = false;
        }

        // DeathLink receive: EMEVD Event 9902 waits on flag 9903.
        if (g_deathLinkPending.exchange(false)) {
            g_suppressNextDeathSend = true;
            WriteEventFlag(ptr1, divisor, 9903, true);
            Log("DeathLink: forcing local death (flag 9903 set)");
        }

        // DeathLink send: EMEVD Event 9900 sets flag 9901 when local HP hits 0.
        if (ReadEventFlag(ptr1, divisor, 9901)) {
            WriteEventFlag(ptr1, divisor, 9901, false);
            if (g_suppressNextDeathSend.exchange(false)) {
                Log("DeathLink: local death was caused by an incoming link - not re-sending");
            } else {
                APClient_SendDeathLink();
            }
        }

        // Location checks
        for (int i = 0; i < g_locationCount; i++) {
            uint32_t flag = g_locations[i].flagId;
            if (g_sentFlags.count(flag)) continue;

            if (ReadEventFlag(ptr1, divisor, flag)) {
                Log("CHECK: [%u] %s", flag, g_locations[i].name);
                g_sentFlags.insert(flag);
                g_checksSent++;

                int64_t baseId = (int64_t)AC6_BASE_ID + flag;

                // Archive flags (4000-4049) are a separate experimental
                bool isArchive = (flag >= 4000 && flag <= 4049);

                Log("flag %u -> \"%s\" => locId %lld%s",
                    flag, g_locations[i].name, (long long)baseId,
                    isArchive ? "" : " (+reward bands)");
                Overlay_Message(OVL_SENT, "Check  %s", g_locations[i].name);

                if (isArchive) {
                    APClient_SendCheck(baseId);
                }
                else {
                    // Send every band; the server ignores ones the seed lacks.
                    for (int band = 0; band < AC6_MAX_MULTIPLIER; band++) {
                        APClient_SendCheck(
                            baseId + (int64_t)band * AC6_MULTIPLIER_OFFSET);
                    }
                }

                // Chapter's final mission: also send Chapter N Complete.
                int endCh = AC6_ChapterEndForFlag(flag);
                if (endCh > 0) {
                    int64_t chLoc = (int64_t)AC6_CHAPTER_COMPLETE_LOC_BASE + endCh;
                    Log("Chapter %d complete => locId %lld",
                        endCh, (long long)chLoc);
                    APClient_SendCheck(chLoc);
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

        // ── Goal ──────────────────────────────────────────────────────────
        // Base game only: any ending flag completes the goal.
        if (!g_goalReported && g_checksSent >= 10) {
            int setCount = 0;
            for (int i = 0; i < g_goalFlagCount; i++)
                if (ReadEventFlag(ptr1, divisor, g_goalFlags[i])) setCount++;
            if (setCount >= 1) {
                Log("GOAL REACHED (ending flag set)");
                Overlay_Message(OVL_INFO, "Goal complete!");
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