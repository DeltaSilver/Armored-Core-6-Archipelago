from worlds.AutoWorld import World
from worlds.generic.Rules import set_rule
from BaseClasses import MultiWorld


def set_rules(world: World, multiworld: MultiWorld, player: int) -> None:
    # ── Progression model ─────────────────────────────────────────────────
    # Chapter/cycle order is enforced by the region graph (create_regions):
    # Chapter 1 -> ... -> Chapter 5 -> NG+ Chapter 1 -> ... -> NG++ Chapter 5.
    # AC6 has no item-based hard gates (you can clear any mission with default
    # gear), so the region chain is the whole progression model — no per-chapter
    # event-flag gates, which were unreliable anyway (chapter mission counts vary
    # and change again in NG+/NG++). Goal/route locations are reachable once
    # their cycle's Chapter 5 is, handled by placement.

    # ── Arena — sequential rank gates ─────────────────────────────────────
    arena_order = ["F", "E", "D", "C", "B", "A", "S"]
    for prev, cur in zip(arena_order, arena_order[1:]):
        set_rule(
            multiworld.get_location(f"Complete Arena {cur}", player),
            lambda state, p=prev: state.can_reach(
                f"Complete Arena {p}", "Location", player),
        )

    # ── Mercenary ranks — sequential ──────────────────────────────────────
    for i in range(2, 18):
        set_rule(
            multiworld.get_location(f"Reach Mercenary Rank {i}", player),
            lambda state, prev=i - 1: state.can_reach(
                f"Reach Mercenary Rank {prev}", "Location", player
            ),
        )
