from Options import Toggle, Choice, PerGameCommonOptions
from dataclasses import dataclass


class ArchiveLogs(Toggle):
    """
    Add the 50 archive log collectibles as extra CHECK locations.

    currently non-functional
    """
    display_name = "Archive Logs as Checks"
    default = 0


class MissionRewardMultiplier(Choice):
    """
    How many item checks each mission/arena/rank objective awards.

    Note: at 4x across all categories the seed uses nearly the entire part
    catalogue (~312 of 314 parts), so there is little variety between seeds.
    Lower values leave more parts rotating in/out each seed.
    """
    display_name = "Mission Reward Multiplier"
    option_1x = 1
    option_2x = 2
    option_3x = 3
    option_4x = 4
    default = 2


class Goal(Choice):
    """
    Which ending counts as goal completion.

    any_ending:   completing Route A, B, or C all count as victory.
    route_a:      only the Fires of Ibis ending counts.
    route_b:      only the Liberator of Rubicon ending counts.
    route_c:      only the Alea Iacta Est ending counts.
    all_endings:  all three routes must be completed (NG++ required).
    """
    display_name = "Goal"
    option_any_ending  = 0
    option_route_a     = 1
    option_route_b     = 2
    option_route_c     = 3
    option_all_endings = 4
    default = 0


@dataclass
class AC6Options(PerGameCommonOptions):
    archive_logs:               ArchiveLogs
    mission_reward_multiplier:  MissionRewardMultiplier
    goal:                       Goal