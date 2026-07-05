from worlds.AutoWorld import World
from worlds.generic.Rules import set_rule
from BaseClasses import MultiWorld

from .locations import CYCLE_NAMES


def _chapter_region(cycle: int, chapter: int) -> str:
    # Must match create_regions in __init__.py.
    prefix = "" if cycle == 0 else f"{CYCLE_NAMES[cycle]} "
    return f"{prefix}Chapter {chapter}"


def set_rules(world: World, multiworld: MultiWorld, player: int) -> None:
    # ── Cycle-access gating (multiworld balance) ───────────────────────────
    # Within a cycle, AC6 has no item-based hard gates (you can clear any
    # mission with default gear), so the region chain is the whole intra-cycle
    # progression model. But the cycle TRANSITIONS must be gated on a real item,
    # or the multiworld solver sees all of NG+/NG++ as reachable from sphere 0
    # and may place other games' early-critical progression behind your NG++
    # finale. So each cycle transition requires its access pass (a progression
    # item create_items adds for multi-cycle modes).
    cycle_pass = {1: "NG+ Access", 2: "NG++ Access"}
    for cyc in range(1, world._num_cycles()):
        entrance = f"{_chapter_region(cyc - 1, 5)} -> {_chapter_region(cyc, 1)}"
        set_rule(
            multiworld.get_entrance(entrance, player),
            lambda state, it=cycle_pass[cyc]: state.has(it, player),
        )

    # ── Chapter-access gating (early->late spheres within a cycle) ─────────
    # Entering Chapter N (in any cycle) requires "Chapter N Access", so Chapter 1
    # (plus the first arena/rank checks) is sphere 0 and later chapters come
    # later. This is what lets the multiworld place other games' early items in
    # AC6's early game instead of treating all of AC6 as immediately reachable.
    for cyc in range(world._num_cycles()):
        for ch in range(2, 6):
            entrance = f"{_chapter_region(cyc, ch - 1)} -> {_chapter_region(cyc, ch)}"
            set_rule(
                multiworld.get_entrance(entrance, player),
                lambda state, it=f"Chapter {ch} Access": state.has(it, player),
            )

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
