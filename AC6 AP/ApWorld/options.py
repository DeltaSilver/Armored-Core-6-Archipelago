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


class RunMode(Choice):
    """
    How many New Game cycles the run spans, and whether each cycle has its own
    checks. (AC6's story flags reset every NG cycle, so the same missions can be
    replayed in NG+ and NG++.)

    single:             one playthrough; reaching any ending completes the goal.
                        Only the NG mission checks exist.
    ng_plus_run:        play NG -> NG+ (both of the first two endings) to
                        complete the goal; each mission's check fires only ONCE.
    ng_plus_run_cycled: play NG -> NG+; every mission fires a SEPARATE check in
                        each cycle (~2x the story checks).
    full_run:           play NG -> NG+ -> NG++ (all three endings) to complete
                        the goal; each mission's check fires only ONCE.
    full_run_cycled:    play NG -> NG+ -> NG++; every mission fires a SEPARATE
                        check in each cycle (~3x the story checks).
    """
    display_name = "Run Mode"
    option_single             = 0
    option_ng_plus_run        = 1
    option_ng_plus_run_cycled = 2
    option_full_run           = 3
    option_full_run_cycled    = 4
    default = 0


@dataclass
class AC6Options(PerGameCommonOptions):
    archive_logs:               ArchiveLogs
    mission_reward_multiplier:  MissionRewardMultiplier
    run_mode:                   RunMode