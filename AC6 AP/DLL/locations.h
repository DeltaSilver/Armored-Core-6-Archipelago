#pragma once
#include <cstdint>

// A location the flag watcher polls. flagId is the in-game event flag;
// name is for logging. The AP location ID sent to the server is
// AC6_BASE_ID + flagId (see apclient.cpp).
struct AC6Location {
    uint32_t    flagId;
    const char* name;
};

// The 86 core locations. Flag IDs match the Python locations.py table.
static const AC6Location g_locations[] = {
    // Story progression — Chapter 1 (14) — verified NG/NG+/NG++; dead
    // placeholders removed, real side-mission flags added. 3409 also fires
    // SetGarageVisited (see flagwatcher.cpp). DLL cycles 3000-3999 per NG cycle.
    {3400, "Chapter 1 Progress 1"},
    {3401, "Chapter 1 Progress 2"},
    {3402, "Chapter 1 Progress 3"},
    {3403, "Chapter 1 Progress 4"},
    {3404, "Chapter 1 Progress 5"},
    {3405, "Chapter 1 Progress 6"},
    {3406, "Chapter 1 Progress 7"},
    {3407, "Chapter 1 Progress 8"},
    {3408, "Chapter 1 Progress 9"},
    {3409, "Chapter 1 Progress 10"},
    {3450, "Chapter 1 Progress 11"},
    {3451, "Chapter 1 Progress 12"},
    {3452, "Chapter 1 Progress 13"},
    {3453, "Chapter 1 Progress 14"},
    // Story progression — Chapter 2 (3 fire; 3413-3419 branch-reserved)
    {3410, "Chapter 2 Progress 1"},
    {3411, "Chapter 2 Progress 2"},
    {3412, "Chapter 2 Progress 3"},
    {3413, "Chapter 2 Progress 4"},
    {3414, "Chapter 2 Progress 5"},
    {3415, "Chapter 2 Progress 6"},
    {3416, "Chapter 2 Progress 7"},
    {3417, "Chapter 2 Progress 8"},
    {3418, "Chapter 2 Progress 9"},
    {3419, "Chapter 2 Progress 10"},
    // Story progression — Chapter 3 (17)
    {3420, "Chapter 3 Progress 1"},
    {3421, "Chapter 3 Progress 2"},
    {3422, "Chapter 3 Progress 3"},
    {3423, "Chapter 3 Progress 4"},
    {3424, "Chapter 3 Progress 5"},
    {3425, "Chapter 3 Progress 6"},
    {3426, "Chapter 3 Progress 7"},
    {3427, "Chapter 3 Progress 8"},
    {3428, "Chapter 3 Progress 9"},
    {3429, "Chapter 3 Progress 10"},
    {3460, "Chapter 3 Progress 11"},
    {3461, "Chapter 3 Progress 12"},
    {3462, "Chapter 3 Progress 13"},
    {3463, "Chapter 3 Progress 14"},
    {3464, "Chapter 3 Progress 15"},
    {3465, "Chapter 3 Progress 16"},
    {3466, "Chapter 3 Progress 17"},
    // Story progression — Chapter 4 (7 fire; 3437-3439 branch-reserved)
    {3430, "Chapter 4 Progress 1"},
    {3431, "Chapter 4 Progress 2"},
    {3432, "Chapter 4 Progress 3"},
    {3433, "Chapter 4 Progress 4"},
    {3434, "Chapter 4 Progress 5"},
    {3435, "Chapter 4 Progress 6"},
    {3436, "Chapter 4 Progress 7"},
    {3437, "Chapter 4 Progress 8"},
    {3438, "Chapter 4 Progress 9"},
    {3439, "Chapter 4 Progress 10"},
    // Story progression — Chapter 5 (5 fire; 3444-3446 branch-reserved)
    {3440, "Chapter 5 Progress 1"},
    {3441, "Chapter 5 Progress 2"},
    {3442, "Chapter 5 Progress 3"},
    {3443, "Chapter 5 Progress 4"},
    {3444, "Chapter 5 Progress 5"},
    {3445, "Chapter 5 Progress 6"},
    {3446, "Chapter 5 Progress 7"},
    {3447, "Chapter 5 Progress 8"},
    // Key missions (one-time; persist across NG cycles)
    {6200, "Chapter 1 Submission"},
    {6210, "Mining Ship and Dam Destruction"},
    {6220, "Over the Wall"},
    {6230, "Coordinates Indicated by the String"},
    {6240, "Continental Crust"},
    {6245, "Old Spaceport Operation"},
    {6250, "Defeat Iceworm"},
    {6275, "Coral Export Denial"},
    {6280, "Coral Convergence"},
    {6260, "Prison Break"},
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
    // Archive logs (EXPERIMENTAL — flag IDs 4000-4049 unverified).
    {4000, "Archive: License Code Thomas Kirk"},
    {4001, "Archive: License Code Monkey Gordo"},
    {4002, "Archive: License Code G7 Hakra"},
    {4003, "Archive: License Code Raven"},
    {4004, "Archive: System Log One-Sided Engagement"},
    {4005, "Archive: System Log The Deserter"},
    {4006, "Archive: Video Record STEEL HAZE"},
    {4007, "Archive: Video Record BAWS Arsenal No. 2"},
    {4008, "Archive: Comms Record Rusty's Comms"},
    {4009, "Archive: Text Data The Well Dries"},
    {4010, "Archive: Video Record Communication Attempt"},
    {4011, "Archive: Comms Record Friendly Comms"},
    {4012, "Archive: Observation Data Coral Density"},
    {4013, "Archive: Observation Data Terrain Survey"},
    {4014, "Archive: Observation Data Installations"},
    {4015, "Archive: Observation Data Offshore Survey"},
    {4016, "Archive: Video Record Rubiconian Invective"},
    {4017, "Archive: Comms Record Doser Ravings"},
    {4018, "Archive: Text Data Dolmayan Writings 1"},
    {4019, "Archive: Text Data Dolmayan Writings 2"},
    {4020, "Archive: Text Data Dolmayan Writings 3"},
    {4021, "Archive: Text Data Dolmayan Writings 4"},
    {4022, "Archive: Text Data Dolmayan Writings 5"},
    {4023, "Archive: Text Data Prof Nagai Log 1"},
    {4024, "Archive: Text Data Prof Nagai Log 2"},
    {4025, "Archive: Text Data Prof Nagai Log 3"},
    {4026, "Archive: Text Data Prof Nagai Log 4"},
    {4027, "Archive: Video Record The Fires of Ibis"},
    {4028, "Archive: Image Data STV Sketch 3"},
    {4029, "Archive: Image Data STV Sketch 4"},
    {4030, "Archive: Image Data STV Sketch 1"},
    {4031, "Archive: Image Data STV Sketch 2"},
    {4032, "Archive: Image Data STV Sketch 6"},
    {4033, "Archive: Image Data STV Sketch 5"},
    {4034, "Archive: Image Data STK Sketch"},
    {4035, "Archive: Observation Data City of Xylem"},
    {4036, "Archive: Video Record G4 Last Words"},
    {4037, "Archive: Text Data Re-education Center"},
    {4038, "Archive: Video Record Testing New Components"},
    {4039, "Archive: Video Record BAWS Guard"},
    {4040, "Archive: Video Record The Collector"},
    {4041, "Archive: Comms Record Message for Uncle"},
    {4042, "Archive: Comms Record Doser Chatter"},
    {4043, "Archive: Comms Record Coyote Chatter"},
    {4044, "Archive: Comms Record Independent Merc"},
    {4045, "Archive: Comms Record Enforcement Squad"},
    {4046, "Archive: Observation Data Wave Mutation"},
    {4047, "Archive: Observation Data Blind Spots"},
    {4048, "Archive: Observation Data Enforcement"},
    {4049, "Archive: Text Data Prof Nagai Log 5"},
};

static const int g_locationCount =
sizeof(g_locations) / sizeof(g_locations[0]);

// Goal flags — any one set means an ending was reached.
static const uint32_t g_goalFlags[] = { 6000, 6001, 6002 };
static const int g_goalFlagCount =
sizeof(g_goalFlags) / sizeof(g_goalFlags[0]);

// Mission reward multiplier bands. Must match MULTIPLIER_OFFSET / MAX_MULTIPLIER
// in the Python locations.py. Each base location can have up to 4 reward
// bands at +0, +500000, +1000000, +1500000. The DLL ALWAYS sends all bands
// for EVERY location flag; the server keeps only the ones the seed generated
// (per the chosen multiplier) and ignores the rest. This means the DLL never
// needs to know the multiplier value.
#define AC6_MULTIPLIER_OFFSET 500000
#define AC6_MAX_MULTIPLIER    4

// New Game cycle offset. Story/mission flags reset and re-fire each NG cycle,
// so the DLL offsets their location IDs by cycle (0=NG, 1=NG+, 2=NG++) to keep
// each cycle's checks distinct. Must exceed the full multiplier span
// (MAX_MULTIPLIER * MULTIPLIER_OFFSET = 2000000) so cycle blocks never overlap.
// Must match CYCLE_OFFSET in the Python locations.py.
#define AC6_CYCLE_OFFSET 2000000

// True for flags that reset and re-fire each NG cycle (story/mission progress
// flags, 3000-3999). Arena (6050+), merc ranks (6400+), key missions (6200+),
// endings, and archives (4000+) persist across cycles and are NOT cycle-offset.
static inline bool AC6_IsCycledFlag(uint32_t flag) {
    return flag >= 3000 && flag <= 3999;
}