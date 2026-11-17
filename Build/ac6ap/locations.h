#pragma once
#include <cstdint>

// Location ID = BASE_ID + flagId (+ band * MULTIPLIER_OFFSET).
struct AC6Location {
    uint32_t    flagId;
    const char* name;
};

// Must match LOCATION_TABLE in locations.py. "A/B" names share one flag.
// Flags confirmed by in-game scanning (2026-07); MIA (3437) unconfirmed.
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
    {3410, "Attack the Watchpoint/Attack the Watchpoint (alt.)"},
    // Chapter 2 — 3413-3419 confirmed dead, removed
    {3411, "Infiltrate Grid 086"},
    {3412, "Eliminate the Doser Faction/Stop the Secret Data Breach"},
    {3420, "Ocean Crossing"},
    // Chapter 3
    {3421, "Steal the Survey Data"},
    {3422, "Attack the Refueling Base"},
    {3423, "Eliminate V.VII"},
    {3424, "Survey the Uninhabited Floating City/Survey the Uninhabited City"},
    {3427, "Tunnel Sabotage/Prevent Corporate Salvage of New Tech"},
    {3428, "Heavy Missile Launch Support"},
    {3429, "Attack the Old Spaceport"},
    {3461, "Eliminate the Enforcement Squads/Destroy the Special Forces Craft"},
    {3462, "Eliminate Honest Brute"},
    {3463, "Defend the Old Spaceport/Defend the Dam Complex"},
    {3464, "Historic Data Recovery/Coral Export Denial"},
    {3430, "Destroy the Ice Worm"},
    // Chapter 4 — 3437-3439 confirmed dead, removed
    {3431, "Underground Exploration: Depth 1"},
    {3432, "Underground Exploration: Depth 2"},
    {3433, "Underground Exploration: Depth 3"},
    {3434, "Ambush the Vespers/Intercept the Redguns/Eliminate V.III"},
    {3435, "Unknown Territory Survey"},
    {3436, "Reach the Coral Convergence/Reach the Coral Convergence (alt.)"},
    {3437, "MIA"},
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
    // Arena
    {6050, "Complete Arena F"},
    {6051, "Complete Arena E"},
    {6052, "Complete Arena D"},
    {6053, "Complete Arena C"},
    {6054, "Complete Arena B"},
    {6055, "Complete Arena A"},
    {6056, "Complete Arena S"},
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

// Chapter N Complete location = base + N, sent on the chapter's final mission.
// Must match CHAPTER_COMPLETE_LOC_BASE / CHAPTER_END_FLAGS in locations.py.
#define AC6_CHAPTER_COMPLETE_LOC_BASE 16700000
static inline int AC6_ChapterEndForFlag(uint32_t flag) {
    switch (flag) {
    case 3410: return 1;   // Attack the Watchpoint
    case 3420: return 2;   // Ocean Crossing
    case 3430: return 3;   // Destroy the Ice Worm
    case 3436: return 4;   // Reach the Coral Convergence
    default:   return 0;
    }
}

// Shop gate flags (must match SHOP_GATE_FLAGS in locations.py):
// Ch1 = first garage visit, Ch2-4 = last mission of the previous chapter.
#define AC6_SHOP_GATE_CH1 3409
#define AC6_SHOP_GATE_CH2 3410
#define AC6_SHOP_GATE_CH3 3420
#define AC6_SHOP_GATE_CH4 3430

// Batch stock flags (eventFlag_forStock in regulation.bin):
// base + chapter*3 + batch. Must match SHOP_BATCH_FLAG_BASE.
#define AC6_SHOP_BATCH_FLAG_BASE 9500
#define AC6_SHOP_CHAPTERS        4
#define AC6_SHOP_BATCHES_PER_CH  3