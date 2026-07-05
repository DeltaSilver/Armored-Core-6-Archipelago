#pragma once
#include <cstdint>

// A location the flag watcher polls. flagId is the in-game event flag;
// name is for logging. The AP location ID the DLL sends is
// BASE_ID + flagId + cycle * CYCLE_OFFSET (see apclient.cpp / flagwatcher.cpp).
struct AC6Location {
    uint32_t    flagId;
    const char* name;
};

// 62 canonical locations matching locations.py exactly.
// Split-path missions use "Name1/Name2" — same flag fires regardless of
// which branch the player took, so one entry covers all variants.
// ALLMIND-questline names are included in the combined string since those
// also reuse the same flag at NG++ (cycle 2).
// Dead/secondary/unmatched flags from the old table are removed; the server
// would have ignored them anyway since they have no matching AP location ID.
static const AC6Location g_locations[] = {
    // Chapter 1
    {3400, "Illegal Entry"},
    {3401, "Destroy Artillery Installations"},
    {3402, "Grid 135 Cleanup"},
    {3403, "Destroy the Transport Helicopters"},
    {3404, "Destroy the Tester AC"},
    {3406, "Attack the Dam Complex/Attack the Dam Complex (alt.)"},
    {3407, "Destroy the Weaponized Mining Ship/Escort the Weaponized Mining Ship"},
    {3450, "Operation Wallclimber"},
    {3451, "Retrieve Combat Logs/Prisoner Rescue"},
    {3452, "Investigate BAWS Arsenal No. 2/Obstruct the Mandatory Inspection"},
    {3453, "Attack the Watchpoint/Attack the Watchpoint (alt.)"},
    // Chapter 2 — 3413-3419 confirmed dead, removed
    {3410, "Infiltrate Grid 086"},
    {3411, "Eliminate the Doser Faction/Stop the Secret Data Breach"},
    {3412, "Ocean Crossing"},
    // Chapter 3
    {3420, "Steal the Survey Data"},
    {3421, "Attack the Refueling Base"},
    {3422, "Eliminate V.VII"},
    {3423, "Survey the Uninhabited Floating City/Survey the Uninhabited City"},
    {3424, "Tunnel Sabotage/Prevent Corporate Salvage of New Tech"},
    {3427, "Heavy Missile Launch Support"},
    {3428, "Attack the Old Spaceport"},
    {3429, "Eliminate the Enforcement Squads/Destroy the Special Forces Craft"},
    {3461, "Eliminate Honest Brute"},
    {3462, "Defend the Old Spaceport/Defend the Dam Complex"},
    {3463, "Historic Data Recovery/Coral Export Denial"},
    {3464, "Destroy the Ice Worm"},
    // Chapter 4 — 3437-3439 confirmed dead, removed
    {3430, "Underground Exploration: Depth 1"},
    {3431, "Underground Exploration: Depth 2"},
    {3432, "Underground Exploration: Depth 3"},
    {3433, "Ambush the Vespers/Intercept the Redguns/Eliminate V.III"},
    {3434, "Unknown Territory Survey"},
    {3435, "Reach the Coral Convergence/Reach the Coral Convergence (alt.)"},
    {3436, "MIA"},
    // Chapter 5 — 3444-3446 confirmed dead, removed
    {3440, "Escape/Regain Control of the Xylem"},
    {3441, "Take the Uninhabited Floating City"},
    {3442, "Intercept the Corporate Forces/Eliminate Cinder Carla"},
    {3443, "Breach the Karman Line/Destroy the Drive Block/Coral Release"},
    {3447, "Shut Down the Closure Satellites/Bring Down the Xylem"},
    // Mercenary ranks
    {6401, "Reach Mercenary Rank 1"},
    {6402, "Reach Mercenary Rank 2"},
    {6403, "Reach Mercenary Rank 3"},
    {6404, "Reach Mercenary Rank 4"},
    {6405, "Reach Mercenary Rank 5"},
    {6406, "Reach Mercenary Rank 6"},
    {6407, "Reach Mercenary Rank 7"},
    {6408, "Reach Mercenary Rank 8"},
    {6409, "Reach Mercenary Rank 9"},
    {6410, "Reach Mercenary Rank 10"},
    {6411, "Reach Mercenary Rank 11"},
    {6412, "Reach Mercenary Rank 12"},
    {6413, "Reach Mercenary Rank 13"},
    {6414, "Reach Mercenary Rank 14"},
    {6415, "Reach Mercenary Rank 15"},
    {6416, "Reach Mercenary Rank 16"},
    {6417, "Reach Mercenary Rank 17"},
    // Arena
    {6050, "Complete Arena F"},
    {6051, "Complete Arena E"},
    {6052, "Complete Arena D"},
    {6053, "Complete Arena C"},
    {6054, "Complete Arena B"},
    {6055, "Complete Arena A"},
    {6056, "Complete Arena S"},
    {6057, "Complete Alpha Simulator"},
    {6058, "Complete Beta Simulator"},
    {6059, "Complete Gamma Simulator"},
};

static const int g_locationCount =
sizeof(g_locations) / sizeof(g_locations[0]);

// Goal flags — any one set means an ending was reached.
static const uint32_t g_goalFlags[] = { 6000, 6001, 6002 };
static const int g_goalFlagCount =
sizeof(g_goalFlags) / sizeof(g_goalFlags[0]);

// Must match MULTIPLIER_OFFSET / MAX_MULTIPLIER in locations.py.
#define AC6_MULTIPLIER_OFFSET 500000
#define AC6_MAX_MULTIPLIER    4

// Must match CYCLE_OFFSET in locations.py.
#define AC6_CYCLE_OFFSET 2000000

// Story/mission flags (3000-3999) reset and re-fire each NG cycle.
// All other flags (arena, ranks, endings) persist and are not cycle-offset.
static inline bool AC6_IsCycledFlag(uint32_t flag) {
    return flag >= 3000 && flag <= 3999;
}

// Shop chapter gate flags — in-game flags that unlock each chapter's shop
// batch when first seen. Must match SHOP_GATE_FLAGS in locations.py and
// flagwatcher.cpp. These are cycled flags but the DLL latches them on first
// fire; the latch persists for the session (game's own flag state is the
// source of truth across sessions).
// Ch1: 3409 (first garage visit)
// Ch2: 3453 (Attack the Watchpoint — last Ch1 mission)
// Ch3: 3412 (Ocean Crossing — last Ch2 mission)
// Ch4: 3464 (Destroy the Ice Worm — last Ch3 mission)
#define AC6_SHOP_GATE_CH1 3409
#define AC6_SHOP_GATE_CH2 3453
#define AC6_SHOP_GATE_CH3 3412
#define AC6_SHOP_GATE_CH4 3464

// Batch-unlock flags — baked into regulation.bin as eventFlag_forStock on
// ShopLineupParam rows. The DLL writes these via WriteEventFlag when a chapter's
// gate fires and batches are needed. The game reads them to decide whether each
// batch of shop items is in stock. One flag per (chapter, batch) pair.
// flag = AC6_SHOP_BATCH_FLAG_BASE + chapter_index*3 + batch_index (0-based)
// flags 9500-9511. Must match SHOP_BATCH_FLAG_BASE in locations.py.
#define AC6_SHOP_BATCH_FLAG_BASE 9500
#define AC6_SHOP_CHAPTERS        4
#define AC6_SHOP_BATCHES_PER_CH  3