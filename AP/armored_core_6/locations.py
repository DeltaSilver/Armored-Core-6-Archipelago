from dataclasses import dataclass


@dataclass
class AC6LocationData:
    code: int
    flag_id: int
    region: str
    questline: str | None = None


BASE_LOC_ID = 7700000


def _loc(flag_id: int, region: str, questline: str | None = None) -> AC6LocationData:
    return AC6LocationData(BASE_LOC_ID + flag_id, flag_id, region, questline)


# Flag IDs confirmed by in-game flag scanning (2026-07). MIA (3437) is unconfirmed.
LOCATION_TABLE: dict[str, AC6LocationData] = {

    # Chapter 1
    "Illegal Entry":                       _loc(3400, "Chapter 1"),
    "Destroy Artillery Installations":     _loc(3401, "Chapter 1"),
    "Grid 135 Cleanup":                    _loc(3402, "Chapter 1"),
    "Destroy the Transport Helicopters":   _loc(3403, "Chapter 1"),
    "Destroy the Tester AC":               _loc(3404, "Chapter 1"),
    "Attack the Dam Complex/Attack the Dam Complex (alt.)": _loc(3406, "Chapter 1"),
    "Destroy the Weaponized Mining Ship/Escort the Weaponized Mining Ship": _loc(3407, "Chapter 1"),
    "Operation Wallclimber":               _loc(3450, "Chapter 1"),
    "Retrieve Combat Logs/Prisoner Rescue": _loc(3451, "Chapter 1"),
    "Investigate BAWS Arsenal No. 2/Obstruct the Mandatory Inspection": _loc(3452, "Chapter 1"),
    "Attack the Watchpoint/Attack the Watchpoint (alt.)": _loc(3410, "Chapter 1"),

    # Chapter 2
    "Infiltrate Grid 086":                 _loc(3411, "Chapter 2"),
    "Eliminate the Doser Faction/Stop the Secret Data Breach": _loc(3412, "Chapter 2"),
    "Ocean Crossing":                      _loc(3420, "Chapter 2"),

    # Chapter 3
    "Steal the Survey Data":               _loc(3421, "Chapter 3"),
    "Attack the Refueling Base":           _loc(3422, "Chapter 3"),
    "Eliminate V.VII":                     _loc(3423, "Chapter 3"),
    "Survey the Uninhabited Floating City/Survey the Uninhabited City": _loc(3424, "Chapter 3"),
    "Tunnel Sabotage/Prevent Corporate Salvage of New Tech": _loc(3427, "Chapter 3"),
    "Heavy Missile Launch Support":        _loc(3428, "Chapter 3"),
    "Attack the Old Spaceport":            _loc(3429, "Chapter 3"),
    "Eliminate the Enforcement Squads/Destroy the Special Forces Craft": _loc(3461, "Chapter 3"),
    "Eliminate Honest Brute":              _loc(3462, "Chapter 3"),
    "Defend the Old Spaceport/Defend the Dam Complex": _loc(3463, "Chapter 3"),
    "Historic Data Recovery/Coral Export Denial": _loc(3464, "Chapter 3"),
    "Destroy the Ice Worm":                _loc(3430, "Chapter 3"),

    # Chapter 4
    "Underground Exploration: Depth 1":    _loc(3431, "Chapter 4"),
    "Underground Exploration: Depth 2":    _loc(3432, "Chapter 4"),
    "Underground Exploration: Depth 3":    _loc(3433, "Chapter 4"),
    "Ambush the Vespers/Intercept the Redguns/Eliminate V.III": _loc(3434, "Chapter 4"),
    "Unknown Territory Survey":            _loc(3435, "Chapter 4"),
    "Reach the Coral Convergence/Reach the Coral Convergence (alt.)": _loc(3436, "Chapter 4"),
    "MIA":                                 _loc(3437, "Chapter 4"),  # unconfirmed

    # Chapter 5
    "Escape/Regain Control of the Xylem":  _loc(3440, "Chapter 5"),
    "Take the Uninhabited Floating City":  _loc(3441, "Chapter 5"),
    "Intercept the Corporate Forces/Eliminate Cinder Carla": _loc(3442, "Chapter 5"),
    "Breach the Karman Line/Destroy the Drive Block/Coral Release": _loc(3443, "Chapter 5"),
    "Shut Down the Closure Satellites/Bring Down the Xylem": _loc(3447, "Chapter 5"),

    # Ranks
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

    # Arena
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
# ---------------------------------------------------------------------------
MULTIPLIER_OFFSET = 500000
MAX_MULTIPLIER = 4
_BAND_SUFFIX = {1: "B", 2: "C", 3: "D"}


def make_multiplier_locations(
        multiplier: int,
        source_table: dict[str, AC6LocationData] | None = None,
) -> dict[str, AC6LocationData]:
    if source_table is None:
        source_table = LOCATION_TABLE
    extra: dict[str, AC6LocationData] = {}
    if multiplier <= 1:
        return extra
    for name, base in source_table.items():
        for band in range(1, multiplier):
            suffix = _BAND_SUFFIX[band]
            extra[f"{name} (Reward {suffix})"] = AC6LocationData(
                base.code + band * MULTIPLIER_OFFSET, base.flag_id, base.region,
                base.questline
            )
    return extra


def all_multiplier_locations() -> dict[str, AC6LocationData]:
    return make_multiplier_locations(MAX_MULTIPLIER)


# Chapter-complete locations, sent by the DLL on each chapter's final mission.
# Real locations (not events) so Universal Tracker only counts them once done.
# Must match AC6_CHAPTER_COMPLETE_LOC_BASE / AC6_ChapterEndForFlag in the DLL.
CHAPTER_COMPLETE_LOC_BASE = BASE_LOC_ID + 9_000_000   # 16,700,000
CHAPTER_END_FLAGS = {
    1: 3410,  # Attack the Watchpoint
    2: 3420,  # Ocean Crossing
    3: 3430,  # Destroy the Ice Worm
    4: 3436,  # Reach the Coral Convergence (MIA 3437 is unconfirmed)
}


def chapter_complete_locations() -> dict[str, AC6LocationData]:
    return {
        f"Chapter {ch} Complete": AC6LocationData(
            CHAPTER_COMPLETE_LOC_BASE + ch, flag, f"Chapter {ch}")
        for ch, flag in CHAPTER_END_FLAGS.items()
    }


# Shops: 4 chapters x 3 batches x 25 items. DLL writes batch flags (9500+)
# when a chapter's gate flag fires; EMEVD sets purchase flags (9600+).
SHOP_LOC_BASE        = 9700000
SHOP_FLAG_BASE       = 9600
SHOP_CHAPTERS        = 4
SHOP_BATCHES_PER_CH  = 3
SHOP_ITEMS_PER_BATCH = 25
SHOP_ITEMS_PER_CH    = SHOP_BATCHES_PER_CH * SHOP_ITEMS_PER_BATCH  # 75
SHOP_SLOTS           = SHOP_CHAPTERS * SHOP_ITEMS_PER_CH            # 300
SHOP_BATCH_FLAG_BASE = 9500  # flags 9500-9511
SHOP_GATE_FLAGS      = [3409, 3410, 3420, 3430]  # Ch1-Ch4


def _shop_loc(slot_0: int) -> AC6LocationData:
    ch = slot_0 // SHOP_ITEMS_PER_CH + 1
    return AC6LocationData(
        SHOP_LOC_BASE + slot_0,
        SHOP_FLAG_BASE + slot_0,
        f"Shop Ch{ch}",
    )


def make_shop_locations(batches_per_ch: int) -> dict[str, AC6LocationData]:
    if batches_per_ch <= 0:
        return {}
    items_per_ch = min(batches_per_ch, SHOP_BATCHES_PER_CH) * SHOP_ITEMS_PER_BATCH
    locs: dict[str, AC6LocationData] = {}
    for ch in range(SHOP_CHAPTERS):
        for i in range(items_per_ch):
            slot_0 = ch * SHOP_ITEMS_PER_CH + i
            locs[f"[Ch{ch + 1}] Shop Item {i + 1}"] = _shop_loc(slot_0)
    return locs


def all_shop_locations() -> dict[str, AC6LocationData]:
    return make_shop_locations(SHOP_BATCHES_PER_CH)