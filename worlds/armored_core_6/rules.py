from worlds.AutoWorld import World
from worlds.generic.Rules import set_rule
from BaseClasses import MultiWorld

from .locations import CYCLE_NAMES, SHOP_CHAPTERS


def _chapter_region(cycle: int, chapter: int) -> str:
    prefix = "" if cycle == 0 else f"{CYCLE_NAMES[cycle]} "
    return f"{prefix}Chapter {chapter}"


def set_rules(world: World, multiworld: MultiWorld, player: int) -> None:

    # -- Cycle transitions -----------------------------------------------
    # Require access passes so NG+/NG++ locations are not sphere 0.
    cycle_pass = {1: "NG+ Access", 2: "NG++ Access"}
    for cyc in range(1, world._num_cycles()):
        entrance = f"{_chapter_region(cyc - 1, 5)} -> {_chapter_region(cyc, 1)}"
        set_rule(
            multiworld.get_entrance(entrance, player),
            lambda state, it=cycle_pass[cyc]: state.has(it, player),
        )

    # -- Chapter sphere gating -------------------------------------------
    # Each chapter requires the previous chapter's locked Complete event.
    # Spheres: NG 1-5, NG+ 6-10, NG++ 11-15.
    for cyc in range(world._num_cycles()):
        for ch in range(1, 5):
            entrance = f"{_chapter_region(cyc, ch)} -> {_chapter_region(cyc, ch + 1)}"
            set_rule(
                multiworld.get_entrance(entrance, player),
                lambda state, it=f"{_chapter_region(cyc, ch)} Complete":
                    state.has(it, player),
            )

    # -- Shop chapter gating ---------------------------------------------
    # Solver-side only; DLL handles the actual in-game unlock.
    if world._compute_batches_per_ch() > 0:
        shop_gates = {
            2: "Chapter 1 Complete",
            3: "Chapter 2 Complete",
            4: "Chapter 3 Complete",
        }
        for ch, required in shop_gates.items():
            set_rule(
                multiworld.get_entrance(f"Menu -> Shop Ch{ch}", player),
                lambda state, it=required: state.has(it, player),
            )

    # -- Arena -----------------------------------------------------------
    # F→E→D→C→B→A→S→Alpha→Beta→Gamma, bounded to active count.
    arena_count = world._arena_count()
    arena_tiers = ["F", "E", "D", "C", "B", "A", "S"]
    for i in range(1, min(arena_count, len(arena_tiers))):
        cur, prev = arena_tiers[i], arena_tiers[i - 1]
        set_rule(
            multiworld.get_location(f"Complete Arena {cur}", player),
            lambda state, p=prev: state.can_reach(f"Complete Arena {p}", "Location", player),
        )
    simulator_order = ["Alpha Simulator", "Beta Simulator", "Gamma Simulator"]
    for i, sim in enumerate(simulator_order):
        if len(arena_tiers) + i >= arena_count:
            break
        if i == 0:
            if arena_count > len(arena_tiers):
                set_rule(
                    multiworld.get_location(f"Complete {sim}", player),
                    lambda state: state.can_reach("Complete Arena S", "Location", player),
                )
        else:
            prev_sim = simulator_order[i - 1]
            set_rule(
                multiworld.get_location(f"Complete {sim}", player),
                lambda state, p=prev_sim: state.can_reach(f"Complete {p}", "Location", player),
            )

    # -- Ranks -----------------------------------------------------------
    # Sequential 1→2→…→N, bounded to active count.
    rank_count = world._rank_count()
    for i in range(2, rank_count + 1):
        set_rule(
            multiworld.get_location(f"Reach Mercenary Rank {i}", player),
            lambda state, prev=i - 1: state.can_reach(
                f"Reach Mercenary Rank {prev}", "Location", player),
        )