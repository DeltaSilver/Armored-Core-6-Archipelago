from worlds.AutoWorld import World
from worlds.generic.Rules import set_rule
from BaseClasses import MultiWorld


def set_rules(world: World, multiworld: MultiWorld, player: int) -> None:

    # ── Region connections ────────────────────────────────────────────────
    # Each chapter unlocks after the previous one's final story flag fires.
    # We use the last progress flag of each chapter as the gate.

    set_rule(
        multiworld.get_entrance("Menu -> Chapter 2", player),
        lambda state: state.can_reach("Chapter 1 First Garage Visit", "Location", player),
    )
    set_rule(
        multiworld.get_entrance("Menu -> Chapter 3", player),
        lambda state: state.can_reach("Chapter 2 Progress 10", "Location", player),
    )
    set_rule(
        multiworld.get_entrance("Menu -> Chapter 4", player),
        lambda state: state.can_reach("Chapter 3 Progress 10", "Location", player),
    )
    set_rule(
        multiworld.get_entrance("Menu -> Chapter 5", player),
        lambda state: state.can_reach("Chapter 4 Progress 10", "Location", player),
    )

    # ── Arena — sequential rank gates ─────────────────────────────────────
    set_rule(
        multiworld.get_location("Complete Arena E", player),
        lambda state: state.can_reach("Complete Arena F", "Location", player),
    )
    set_rule(
        multiworld.get_location("Complete Arena D", player),
        lambda state: state.can_reach("Complete Arena E", "Location", player),
    )
    set_rule(
        multiworld.get_location("Complete Arena C", player),
        lambda state: state.can_reach("Complete Arena D", "Location", player),
    )
    set_rule(
        multiworld.get_location("Complete Arena B", player),
        lambda state: state.can_reach("Complete Arena C", "Location", player),
    )
    set_rule(
        multiworld.get_location("Complete Arena A", player),
        lambda state: state.can_reach("Complete Arena B", "Location", player),
    )
    set_rule(
        multiworld.get_location("Complete Arena S", player),
        lambda state: state.can_reach("Complete Arena A", "Location", player),
    )

    # ── Mercenary ranks — sequential ──────────────────────────────────────
    for i in range(2, 18):
        set_rule(
            multiworld.get_location(f"Reach Mercenary Rank {i}", player),
            lambda state, prev=i - 1: state.can_reach(
                f"Reach Mercenary Rank {prev}", "Location", player
            ),
        )

    # ── Key missions — chapter gated ──────────────────────────────────────
    set_rule(
        multiworld.get_location("Chapter 1 Submission", player),
        lambda state: state.can_reach("Chapter 1", "Region", player),
    )
    set_rule(
        multiworld.get_location("Mining Ship and Dam Destruction", player),
        lambda state: state.can_reach("Chapter 2", "Region", player),
    )
    set_rule(
        multiworld.get_location("Over the Wall", player),
        lambda state: state.can_reach("Chapter 2", "Region", player),
    )
    set_rule(
        multiworld.get_location("Coordinates Indicated by the String", player),
        lambda state: state.can_reach("Chapter 3", "Region", player),
    )
    set_rule(
        multiworld.get_location("Continental Crust", player),
        lambda state: state.can_reach("Chapter 3", "Region", player),
    )
    set_rule(
        multiworld.get_location("Defeat Iceworm", player),
        lambda state: state.can_reach("Chapter 4", "Region", player),
    )
    set_rule(
        multiworld.get_location("Prison Break", player),
        lambda state: state.can_reach("Chapter 5", "Region", player),
    )

    # ── Goal locations — all require Chapter 5 ────────────────────────────
    for route in ("Route A Clear", "Route B Clear", "Route C Clear"):
        if multiworld.get_location(route, player) is not None:
            set_rule(
                multiworld.get_location(route, player),
                lambda state: state.can_reach("Chapter 5", "Region", player),
            )