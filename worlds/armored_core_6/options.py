from Options import Toggle, Choice, Range, PerGameCommonOptions
from dataclasses import dataclass


class MissionRewardMultiplier(Choice):
    """
    How many item checks each mission/arena/rank objective awards.
    At 4x the seed uses nearly the entire part catalogue, leaving little
    variety between seeds. Lower values give more rotation per seed.
    """
    display_name = "Mission Reward Multiplier"
    option_1x = 1
    option_2x = 2
    option_3x = 3
    option_4x = 4
    default = 2


class RunMode(Choice):
    """
    How many New Game cycles the run spans, and whether each cycle has its
    own set of checks.

    single:             one playthrough, any ending completes the goal.
    ng_plus_run:        NG → NG+, each mission checks once total.
    ng_plus_run_cycled: NG → NG+, each mission fires a separate check per cycle.
    full_run:           NG → NG+ → NG++, each mission checks once total.
    full_run_cycled:    NG → NG+ → NG++, each mission fires a separate check per cycle.
    """
    display_name = "Run Mode"
    option_single             = 0
    # option_ng_plus_run        = 1
    # option_ng_plus_run_cycled = 2
    # option_full_run           = 3
    # option_full_run_cycled    = 4
    default = 0


class MercenaryRankChecks(Range):
    """
    How many mercenary rank checks to include (0-17).
    Order: 1 -> 2 -> ... -> 17. Set to 0 to disable.
    """
    display_name = "Mercenary Rank Checks"
    range_start = 0
    range_end   = 17
    default     = 17


class ArenaChecks(Range):
    """
    How many arena checks to include (0-10).
    Order: 1=F -> 2=E -> 3=D -> 4=C -> 5=B -> 6=A -> 7=S -> 8=Alpha Simulator -> 9=Beta Simulator -> 10=Gamma Simulator
    Set to 0 to disable.
    """
    display_name = "Arena Checks"
    range_start = 0
    range_end   = 10
    default     = 10


class ShopChecks(Toggle):
    """
    Add shop purchases as check locations. Shop slots absorb any remaining
    parts after mission/arena/rank locations are filled, up to 300 slots
    across 4 chapters. Slot mapping is sent via slot_data; no per-seed files
    are needed. Shop checks are cycle-agnostic and fire exactly once.
    """
    display_name = "Shop Checks"
    default = 0


# ---------------------------------------------------------------------------
# Options dataclass
# ---------------------------------------------------------------------------
@dataclass
class AC6Options(PerGameCommonOptions):
    mission_reward_multiplier:  MissionRewardMultiplier
    run_mode:                   RunMode
    shop_checks:                ShopChecks
    mercenary_rank_checks:      MercenaryRankChecks
    arena_checks:               ArenaChecks