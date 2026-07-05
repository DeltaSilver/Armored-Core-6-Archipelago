from dataclasses import dataclass


@dataclass
class AC6LocationData:
    code: int       # AP location ID
    flag_id: int    # in-game event flag
    region: str     # which AP region this belongs to


# Base ID for all AC6 locations — must not overlap with item BASE_ID.
BASE_LOC_ID = 7700000

# Story counter flags that never fired on any tested route (NG/NG+/NG++) — they
# are referenced in the mission controller for branch choices not taken. Their
# checks can therefore strand items, so they are excluded from progression in
# every mode and omitted entirely from single-run seeds (see __init__.py).
# Redundant story counters that co-fire with another counter on the SAME mission
# (verified consistent across the NG and NG++ discovery runs). Dropped as checks
# so each mission yields exactly one story check. 3409 also drives the DLL garage
# gate, which the DLL reads directly (not as a check).
COLLAPSED_FLAGS = frozenset({
    3405, 3408, 3409, 3453,        # Chapter 1
    3425, 3426, 3460, 3465, 3466,  # Chapter 3
})

BRANCH_RESERVED_FLAGS = frozenset({
    3413, 3414, 3415, 3416, 3417, 3418, 3419,   # Chapter 2
    3437, 3438, 3439,                            # Chapter 4
    3444, 3445, 3446,                            # Chapter 5
})


def _loc(flag_id: int, region: str) -> AC6LocationData:
    return AC6LocationData(BASE_LOC_ID + flag_id, flag_id, region)


# ---------------------------------------------------------------------------
# Core locations (always active) — 78 total
# ---------------------------------------------------------------------------
LOCATION_TABLE: dict[str, AC6LocationData] = {

    # -- Story progression (Chapters 1-5) -- ONE CHECK PER MISSION --
    # The 34xx flags are positional story counters: clearing one mission can tick
    # 2-3 of them at once, so they are NOT 1:1 with missions. The redundant
    # co-firing ticks (COLLAPSED_FLAGS) are dropped here so each mission maps to
    # exactly one check, which also lets the names line up. Co-fire groups and
    # names were taken from the logged discovery runs (NG / NG+ / NG++); branch
    # DECISION points are joined "A/B". Chapter 2-5 follow the observed route-A
    # order; Chapter 1's exact order is a best guess (it was only logged in NG++).
    # flag 3409 (intro/garage) is collapsed into 3400 but the DLL still reads it
    # directly for the first-garage grant gate. Renaming a check needs no DLL
    # change (the DLL keys off flag IDs).

    # -- Chapter 1 --
    "Illegal Entry":                              _loc(3400, "Chapter 1"),
    "Grid 135 Cleanup":                           _loc(3401, "Chapter 1"),
    "Destroy Artillery Installations":            _loc(3402, "Chapter 1"),
    "Destroy the Transport Helicopters":          _loc(3403, "Chapter 1"),
    "Destroy the Tester AC":                      _loc(3404, "Chapter 1"),
    "Attack the Dam Complex":                     _loc(3406, "Chapter 1"),
    "Destroy/Escort the Weaponized Mining Ship":  _loc(3407, "Chapter 1"),
    "Operation Wallclimber":                      _loc(3450, "Chapter 1"),
    "Retrieve Combat Logs":                       _loc(3451, "Chapter 1"),
    "Investigate BAWS Arsenal No. 2":             _loc(3452, "Chapter 1"),
    # NOTE: "Attack the Watchpoint" is the Ch1 finale and fires NO story counter
    # (clearing it flips the chapter marker 3000->3002), so it has no check here.

    # -- Chapter 2 --
    "Infiltrate Grid 086":                                      _loc(3410, "Chapter 2"),
    "Eliminate the Doser Faction/Stop the Secret Data Breach":  _loc(3411, "Chapter 2"),
    "Ocean Crossing":                                           _loc(3412, "Chapter 2"),

    # -- Chapter 3 --
    "Steal the Survey Data":                                              _loc(3420, "Chapter 3"),
    "Attack the Refueling Base":                                          _loc(3421, "Chapter 3"),
    "Eliminate V.VII":                                                    _loc(3422, "Chapter 3"),
    "Survey the Uninhabited Floating City":                               _loc(3423, "Chapter 3"),
    "Tunnel Sabotage/Prevent Corporate Salvage of New Tech":              _loc(3424, "Chapter 3"),
    "Heavy Missile Launch Support":                                       _loc(3427, "Chapter 3"),
    "Attack the Old Spaceport":                                           _loc(3428, "Chapter 3"),
    "Eliminate the Enforcement Squads/Destroy the Special Forces Craft":  _loc(3429, "Chapter 3"),
    "Eliminate Honest Brute":                                             _loc(3461, "Chapter 3"),
    "Defend the Old Spaceport":                                           _loc(3462, "Chapter 3"),
    "Historic Data Recovery":                                             _loc(3463, "Chapter 3"),
    "Destroy the Ice Worm":                                               _loc(3464, "Chapter 3"),

    # -- Chapter 4 --
    "Underground Exploration - Depth 1":                         _loc(3430, "Chapter 4"),
    "Underground Exploration - Depth 2":                         _loc(3431, "Chapter 4"),
    "Underground Exploration - Depth 3":                         _loc(3432, "Chapter 4"),
    "Intercept the Redguns/Ambush the Vespers":                  _loc(3433, "Chapter 4"),
    "Unknown Territory Survey":                                  _loc(3434, "Chapter 4"),
    "Reach the Coral Convergence":                               _loc(3435, "Chapter 4"),
    # 3436 dropped: it only fires as the NG++ Ch5 opener (MIA), never in base Ch4.

    # -- Chapter 5 --
    # Names follow the base (route-A / NG) Chapter 5. MIA, Regain Control of the
    # Xylem and Coral Release are NG++ (Allmind) variants and are NOT used here;
    # the route-A opener is "Escape". The finale (3447) is route-dependent, so it
    # carries all three ending missions joined.
    "Escape":                                                               _loc(3440, "Chapter 5"),
    "Take the Uninhabited Floating City":                                   _loc(3441, "Chapter 5"),
    "Intercept the Corporate Forces/Eliminate Cinder Carla":                _loc(3442, "Chapter 5"),
    "Breach the Karman Line":                                               _loc(3443, "Chapter 5"),
    "Shut Down the Closure Satellites/Bring Down the Xylem/Coral Release":  _loc(3447, "Chapter 5"),

    # -- Key missions (one-time story milestones; persist across NG cycles) --
    # Confirmed firing on these missions across the playthroughs. 6245/6275/6280
    # are real flags the old table was missing; chapters corrected (6210/6220
    # fire in Chapter 1, not 2).
    "Chapter 1 Submission":                  _loc(6200, "Chapter 1"),
    "Mining Ship and Dam Destruction":       _loc(6210, "Chapter 1"),
    "Over the Wall":                         _loc(6220, "Chapter 1"),
    "Coordinates Indicated by the String":   _loc(6230, "Chapter 2"),
    "Continental Crust":                     _loc(6240, "Chapter 3"),
    "Old Spaceport Operation":               _loc(6245, "Chapter 3"),
    "Defeat Iceworm":                        _loc(6250, "Chapter 3"),
    "Coral Export Denial":                   _loc(6275, "Chapter 3"),
    "Coral Convergence":                     _loc(6280, "Chapter 4"),
    "Prison Break":                          _loc(6260, "Chapter 5"),

    # -- Mercenary ranks --
    "Reach Mercenary Rank 1":         _loc(6401, "Ranks"),
    "Reach Mercenary Rank 2":         _loc(6402, "Ranks"),
    "Reach Mercenary Rank 3":         _loc(6403, "Ranks"),
    "Reach Mercenary Rank 4":         _loc(6404, "Ranks"),
    "Reach Mercenary Rank 5":         _loc(6405, "Ranks"),
    "Reach Mercenary Rank 6":         _loc(6406, "Ranks"),
    "Reach Mercenary Rank 7":         _loc(6407, "Ranks"),
    "Reach Mercenary Rank 8":         _loc(6408, "Ranks"),
    "Reach Mercenary Rank 9":         _loc(6409, "Ranks"),
    "Reach Mercenary Rank 10":        _loc(6410, "Ranks"),
    "Reach Mercenary Rank 11":        _loc(6411, "Ranks"),
    "Reach Mercenary Rank 12":        _loc(6412, "Ranks"),
    "Reach Mercenary Rank 13":        _loc(6413, "Ranks"),
    "Reach Mercenary Rank 14":        _loc(6414, "Ranks"),
    "Reach Mercenary Rank 15":        _loc(6415, "Ranks"),
    "Reach Mercenary Rank 16":        _loc(6416, "Ranks"),
    "Reach Mercenary Rank 17":        _loc(6417, "Ranks"),

    # -- Arena --
    "Complete Arena F":               _loc(6050, "Arena"),
    "Complete Arena E":               _loc(6051, "Arena"),
    "Complete Arena D":               _loc(6052, "Arena"),
    "Complete Arena C":               _loc(6053, "Arena"),
    "Complete Arena B":               _loc(6054, "Arena"),
    "Complete Arena A":               _loc(6055, "Arena"),
    "Complete Arena S":               _loc(6056, "Arena"),
}

# ---------------------------------------------------------------------------
# Mission reward multiplier
#
# Each base location can award up to 4 items by creating up to 3 extra "(Reward
# B/C/D)" locations that share the same in-game flag but use distinct AP IDs,
# offset by multiples of MULTIPLIER_OFFSET. Applies to ALL core categories
# (story, key missions, arena, ranks).
#
# Band layout (offset from the base location ID):
#   Reward A = +0              (the base location itself)
#   Reward B = +500000
#   Reward C = +1000000
#   Reward D = +1500000
# Highest base flag is ~6417, so bands 500000 apart never collide.
# ---------------------------------------------------------------------------
MULTIPLIER_OFFSET = 500000
MAX_MULTIPLIER = 4

# Suffix letters for the 2nd/3rd/4th copies (band index 1,2,3).
_BAND_SUFFIX = {1: "B", 2: "C", 3: "D"}


def make_multiplier_locations(multiplier: int) -> dict[str, AC6LocationData]:
    """Extra reward locations for a given multiplier (2x-4x).

    For multiplier N, creates N-1 extra locations per base location, each
    sharing the base's flag but offset by band * MULTIPLIER_OFFSET. The DLL
    always sends all bands for a flag; the server keeps only the ones a given
    seed generated, so this stays in sync without the DLL knowing N.
    """
    extra: dict[str, AC6LocationData] = {}
    if multiplier <= 1:
        return extra
    for name, base in LOCATION_TABLE.items():
        for band in range(1, multiplier):          # 1..N-1
            suffix = _BAND_SUFFIX[band]
            extra[f"{name} (Reward {suffix})"] = AC6LocationData(
                base.code + band * MULTIPLIER_OFFSET, base.flag_id, base.region
            )
    return extra


def all_multiplier_locations() -> dict[str, AC6LocationData]:
    """Every possible multiplier location (all bands up to MAX_MULTIPLIER).

    Used to register every location ID in the datapackage so it is stable
    regardless of which multiplier a given player picks.
    """
    return make_multiplier_locations(MAX_MULTIPLIER)


# ---------------------------------------------------------------------------
# New Game cycle (NG / NG+ / NG++)
#
# Story/mission progress flags reset and re-fire on every NG cycle, so each
# cycle gets its OWN set of checks for the same missions. The DLL offsets a
# story flag's location ID by cycle * CYCLE_OFFSET (see AC6_CYCLE_OFFSET in the
# C++ locations.h — the values MUST match). Persistent categories (key
# missions, arena, ranks, archives) fire once and are not duplicated.
#
# Layout: location ID = BASE_LOC_ID + cycle*CYCLE_OFFSET + band*MULTIPLIER_OFFSET
#                       + flag_id.  CYCLE_OFFSET exceeds the full multiplier span
# (MAX_MULTIPLIER * MULTIPLIER_OFFSET = 2,000,000) so cycle blocks never collide.
# ---------------------------------------------------------------------------
CYCLE_OFFSET = 2_000_000          # MUST match AC6_CYCLE_OFFSET in the DLL
NUM_CYCLES = 3
CYCLE_NAMES = {0: "NG", 1: "NG+", 2: "NG++"}


def is_cycled_flag(flag_id: int) -> bool:
    """Story/mission flags (3000-3999) reset each NG cycle; everything else
    persists and is checked only once."""
    return 3000 <= flag_id <= 3999


def add_cycles(table: dict[str, AC6LocationData],
               num_cycles: int = NUM_CYCLES) -> dict[str, AC6LocationData]:
    """Expand a (band-expanded) cycle-0 table with extra-cycle copies of every
    cycled (story) location, up to num_cycles total. Cycle 0 (NG) entries keep
    their names/regions; later-cycle copies are offset by cycle*CYCLE_OFFSET,
    suffixed, and tagged into per-cycle regions ('NG+ Chapter 1', ...). Non-
    cycled locations pass through. Use num_cycles=NUM_CYCLES for the full
    datapackage registration; pass the active run's cycle count otherwise.
    """
    out: dict[str, AC6LocationData] = {}
    for name, d in table.items():
        out[name] = d  # cycle 0 (NG) — unchanged
        if is_cycled_flag(d.flag_id):
            for cyc in range(1, num_cycles):
                out[f"{name} ({CYCLE_NAMES[cyc]})"] = AC6LocationData(
                    d.code + cyc * CYCLE_OFFSET, d.flag_id,
                    f"{CYCLE_NAMES[cyc]} {d.region}")
    return out


# ---------------------------------------------------------------------------
# Archive log locations (only added when the archive_logs option is enabled)
# Checks only -- collecting a log in-game fires the flag and sends a check.
# DOES NOT WORK -- NO OPTION IS GIVEN TO ENABLE
# ---------------------------------------------------------------------------
ARCHIVE_LOG_LOCATIONS: dict[str, AC6LocationData] = {
    "Archive: License Code Thomas Kirk":             _loc(4000, "Chapter 1"),
    "Archive: License Code Monkey Gordo":            _loc(4001, "Chapter 1"),
    "Archive: License Code G7 Hakra":                _loc(4002, "Chapter 1"),
    "Archive: License Code Raven":                   _loc(4003, "Chapter 1"),
    "Archive: System Log One-Sided Engagement":      _loc(4004, "Chapter 1"),
    "Archive: System Log The Deserter":              _loc(4005, "Chapter 1"),
    "Archive: Video Record STEEL HAZE":              _loc(4006, "Chapter 2"),
    "Archive: Video Record BAWS Arsenal No. 2":      _loc(4007, "Chapter 2"),
    "Archive: Comms Record Rusty's Comms":           _loc(4008, "Chapter 2"),
    "Archive: Text Data The Well Dries":             _loc(4009, "Chapter 2"),
    "Archive: Video Record Communication Attempt":   _loc(4010, "Chapter 2"),
    "Archive: Comms Record Friendly Comms":          _loc(4011, "Chapter 2"),
    "Archive: Observation Data Coral Density":       _loc(4012, "Chapter 2"),
    "Archive: Observation Data Terrain Survey":      _loc(4013, "Chapter 2"),
    "Archive: Observation Data Installations":       _loc(4014, "Chapter 3"),
    "Archive: Observation Data Offshore Survey":     _loc(4015, "Chapter 3"),
    "Archive: Video Record Rubiconian Invective":    _loc(4016, "Chapter 3"),
    "Archive: Comms Record Doser Ravings":           _loc(4017, "Chapter 3"),
    "Archive: Text Data Dolmayan Writings 1":        _loc(4018, "Chapter 3"),
    "Archive: Text Data Dolmayan Writings 2":        _loc(4019, "Chapter 3"),
    "Archive: Text Data Dolmayan Writings 3":        _loc(4020, "Chapter 3"),
    "Archive: Text Data Dolmayan Writings 4":        _loc(4021, "Chapter 3"),
    "Archive: Text Data Dolmayan Writings 5":        _loc(4022, "Chapter 3"),
    "Archive: Text Data Prof Nagai Log 1":           _loc(4023, "Chapter 3"),
    "Archive: Text Data Prof Nagai Log 2":           _loc(4024, "Chapter 3"),
    "Archive: Text Data Prof Nagai Log 3":           _loc(4025, "Chapter 4"),
    "Archive: Text Data Prof Nagai Log 4":           _loc(4026, "Chapter 4"),
    "Archive: Video Record The Fires of Ibis":       _loc(4027, "Chapter 4"),
    "Archive: Image Data STV Sketch 3":              _loc(4028, "Chapter 4"),
    "Archive: Image Data STV Sketch 4":              _loc(4029, "Chapter 4"),
    "Archive: Image Data STV Sketch 1":              _loc(4030, "Chapter 4"),
    "Archive: Image Data STV Sketch 2":              _loc(4031, "Chapter 4"),
    "Archive: Image Data STV Sketch 6":              _loc(4032, "Chapter 4"),
    "Archive: Image Data STV Sketch 5":              _loc(4033, "Chapter 4"),
    "Archive: Image Data STK Sketch":                _loc(4034, "Chapter 4"),
    "Archive: Observation Data City of Xylem":       _loc(4035, "Chapter 5"),
    "Archive: Video Record G4 Last Words":           _loc(4036, "Chapter 5"),
    "Archive: Text Data Re-education Center":         _loc(4037, "Chapter 5"),
    "Archive: Video Record Testing New Components":   _loc(4038, "Chapter 5"),
    "Archive: Video Record BAWS Guard":              _loc(4039, "Chapter 5"),
    "Archive: Video Record The Collector":           _loc(4040, "Chapter 5"),
    "Archive: Comms Record Message for Uncle":       _loc(4041, "Chapter 5"),
    "Archive: Comms Record Doser Chatter":           _loc(4042, "Chapter 5"),
    "Archive: Comms Record Coyote Chatter":          _loc(4043, "Chapter 5"),
    "Archive: Comms Record Independent Merc":        _loc(4044, "Chapter 5"),
    "Archive: Comms Record Enforcement Squad":       _loc(4045, "Chapter 5"),
    "Archive: Observation Data Wave Mutation":       _loc(4046, "Chapter 5"),
    "Archive: Observation Data Blind Spots":         _loc(4047, "Chapter 5"),
    "Archive: Observation Data Enforcement":         _loc(4048, "Chapter 5"),
    "Archive: Text Data Prof Nagai Log 5":           _loc(4049, "Chapter 5"),
}