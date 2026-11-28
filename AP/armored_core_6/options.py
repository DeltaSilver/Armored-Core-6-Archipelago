from Options import Toggle, Choice, Range, PerGameCommonOptions, DeathLink
from dataclasses import dataclass


class MissionRewardMultiplier(Choice):
    """
    How many checks each mission, arena rank and mercenary rank gives.
    Higher values use more of the part catalogue per seed.
    """
    display_name = "Mission Reward Multiplier"
    option_1x = 1
    option_2x = 2
    option_3x = 3
    option_4x = 4
    default = 2


class MercenaryRankChecks(Range):
    """
    How many mercenary rank checks to include (0-15). 0 disables them.
    Some Combat Logs are NG+ only, so the top ranks may be unreachable in a
    single playthrough; raise this at your own risk.
    """
    display_name = "Mercenary Rank Checks"
    range_start = 0
    range_end   = 15
    default     = 0


class ArenaChecks(Range):
    """
    How many arena ranks to include, from F up to S (0-7). 0 disables them.
    """
    display_name = "Arena Checks"
    range_start = 0
    range_end   = 7
    default     = 7


class ShopChecks(Toggle):
    """
    Add shop purchases as checks. Shops hold the parts left over after
    missions, arena and ranks (up to 300 slots across 4 chapters).
    """
    display_name = "Shop Checks"
    default = 0


class ShopProgressionPercent(Range):
    """
    Percentage of shop slots (picked at random) that can hold progression
    items. The rest only get parts or filler. 0 keeps key items out of
    shops; 100 treats shops like any other check.
    """
    display_name = "Shop Progression Slots (%)"
    range_start = 0
    range_end   = 100
    default     = 10


@dataclass
class AC6Options(PerGameCommonOptions):
    mission_reward_multiplier:  MissionRewardMultiplier
    # Shops are disabled (see SHOP_CHECKS_ENABLED in __init__.py); uncomment
    # these to expose them in the YAML again.
    # shop_checks:                ShopChecks
    # shop_progression_percent:   ShopProgressionPercent
    mercenary_rank_checks:      MercenaryRankChecks
    arena_checks:                ArenaChecks
    # death_link:                 DeathLink