from dataclasses import dataclass


@dataclass
class AC6LocationData:
    code: int       # AP location ID
    flag_id: int    # in-game event flag
    region: str     # which AP region this belongs to


# Base ID for all AC6 locations — must not overlap with item BASE_ID.
BASE_LOC_ID = 7700000


def _loc(flag_id: int, region: str) -> AC6LocationData:
    return AC6LocationData(BASE_LOC_ID + flag_id, flag_id, region)


# ---------------------------------------------------------------------------
# Core locations (always active) — 78 total
# ---------------------------------------------------------------------------
LOCATION_TABLE: dict[str, AC6LocationData] = {

    # -- Story progression (Chapter 1) --
    "Chapter 1 Progress 1":           _loc(3400, "Chapter 1"),
    "Chapter 1 Progress 2":           _loc(3401, "Chapter 1"),
    "Chapter 1 Progress 3":           _loc(3402, "Chapter 1"),
    "Chapter 1 Progress 4":           _loc(3403, "Chapter 1"),
    "Chapter 1 Progress 5":           _loc(3404, "Chapter 1"),
    "Chapter 1 Progress 6":           _loc(3405, "Chapter 1"),
    "Chapter 1 Progress 7":           _loc(3406, "Chapter 1"),
    "Chapter 1 Progress 8":           _loc(3407, "Chapter 1"),
    "Chapter 1 Progress 9":           _loc(3408, "Chapter 1"),
    "Chapter 1 First Garage Visit":   _loc(3409, "Chapter 1"),

    # -- Story progression (Chapter 2) --
    "Chapter 2 Progress 1":           _loc(3410, "Chapter 2"),
    "Chapter 2 Progress 2":           _loc(3411, "Chapter 2"),
    "Chapter 2 Progress 3":           _loc(3412, "Chapter 2"),
    "Chapter 2 Progress 4":           _loc(3413, "Chapter 2"),
    "Chapter 2 Progress 5":           _loc(3414, "Chapter 2"),
    "Chapter 2 Progress 6":           _loc(3415, "Chapter 2"),
    "Chapter 2 Progress 7":           _loc(3416, "Chapter 2"),
    "Chapter 2 Progress 8":           _loc(3417, "Chapter 2"),
    "Chapter 2 Progress 9":           _loc(3418, "Chapter 2"),
    "Chapter 2 Progress 10":          _loc(3419, "Chapter 2"),

    # -- Story progression (Chapter 3) --
    "Chapter 3 Progress 1":           _loc(3420, "Chapter 3"),
    "Chapter 3 Progress 2":           _loc(3421, "Chapter 3"),
    "Chapter 3 Progress 3":           _loc(3422, "Chapter 3"),
    "Chapter 3 Progress 4":           _loc(3423, "Chapter 3"),
    "Chapter 3 Progress 5":           _loc(3424, "Chapter 3"),
    "Chapter 3 Progress 6":           _loc(3425, "Chapter 3"),
    "Chapter 3 Progress 7":           _loc(3426, "Chapter 3"),
    "Chapter 3 Progress 8":           _loc(3427, "Chapter 3"),
    "Chapter 3 Progress 9":           _loc(3428, "Chapter 3"),
    "Chapter 3 Progress 10":          _loc(3429, "Chapter 3"),

    # -- Story progression (Chapter 4) --
    "Chapter 4 Progress 1":           _loc(3430, "Chapter 4"),
    "Chapter 4 Progress 2":           _loc(3431, "Chapter 4"),
    "Chapter 4 Progress 3":           _loc(3432, "Chapter 4"),
    "Chapter 4 Progress 4":           _loc(3433, "Chapter 4"),
    "Chapter 4 Progress 5":           _loc(3434, "Chapter 4"),
    "Chapter 4 Progress 6":           _loc(3435, "Chapter 4"),
    "Chapter 4 Progress 7":           _loc(3436, "Chapter 4"),
    "Chapter 4 Progress 8":           _loc(3437, "Chapter 4"),
    "Chapter 4 Progress 9":           _loc(3438, "Chapter 4"),
    "Chapter 4 Progress 10":          _loc(3439, "Chapter 4"),

    # -- Story progression (Chapter 5) --
    "Chapter 5 Progress 1":           _loc(3440, "Chapter 5"),
    "Chapter 5 Progress 2":           _loc(3441, "Chapter 5"),
    "Chapter 5 Progress 3":           _loc(3442, "Chapter 5"),
    "Chapter 5 Progress 4":           _loc(3443, "Chapter 5"),
    "Chapter 5 Progress 5":           _loc(3444, "Chapter 5"),
    "Chapter 5 Progress 6":           _loc(3445, "Chapter 5"),
    "Chapter 5 Progress 7":           _loc(3446, "Chapter 5"),

    # -- Key missions --
    "Chapter 1 Submission":                  _loc(6200, "Chapter 1"),
    "Mining Ship and Dam Destruction":       _loc(6210, "Chapter 2"),
    "Over the Wall":                         _loc(6220, "Chapter 2"),
    "Coordinates Indicated by the String":   _loc(6230, "Chapter 3"),
    "Continental Crust":                     _loc(6240, "Chapter 3"),
    "Defeat Iceworm":                        _loc(6250, "Chapter 4"),
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