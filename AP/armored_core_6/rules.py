from worlds.AutoWorld import World
from worlds.generic.Rules import set_rule, add_item_rule
from BaseClasses import MultiWorld, LocationProgressType

from .locations import SHOP_CHAPTERS


def _with_bands(active_names, base: str) -> list:
    """The base location plus its active "(Reward B/C/D)" copies."""
    return [n for n in active_names
            if n == base or n.startswith(f"{base} (Reward ")]


def set_rules(world: World, multiworld: MultiWorld, player: int) -> None:
    active_locs = world._active_locations()
    active_names = set(active_locs)

    # Chapter gating (soft logic): Chapter N+1 needs the locked "Chapter N
    # Complete" item, so each chapter is one sphere later. Doesn't constrain
    # fill, but balancing and Universal Tracker see it.
    for ch in range(1, 5):
        set_rule(
            multiworld.get_entrance(f"Chapter {ch} -> Chapter {ch + 1}", player),
            lambda state, it=f"Chapter {ch} Complete": state.has(it, player),
        )

    if world._compute_batches_per_ch() > 0:
        # Shop Ch2-4 open after the previous chapter (DLL does the in-game unlock).
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

        # Only shop_progression_percent% of slots may hold progression.
        # Item rule, not EXCLUDED: the pool is mostly useful parts, so
        # excluding would run out of filler. Sorted so the pick is seed-stable.
        shop_names = sorted(n for n, d in active_locs.items()
                            if d.region.startswith("Shop Ch"))
        pct = int(world.options.shop_progression_percent.value)
        allowed = round(len(shop_names) * pct / 100)
        world.random.shuffle(shop_names)
        for name in shop_names[allowed:]:
            add_item_rule(multiworld.get_location(name, player),
                          lambda item: not item.advancement)

    # Arena: each rank opens at a story mission (per powerpyx's arena guide).
    # E/D/A open on a chapter's final mission, so they use its Complete item.
    ARENA_UNLOCK_MISSION = {
        "F": "Retrieve Combat Logs/Prisoner Rescue",
        "C": "Tunnel Sabotage/Prevent Corporate Salvage of New Tech",
        "B": "Attack the Old Spaceport",
        "S": "Underground Exploration: Depth 3",
    }
    ARENA_UNLOCK_CHAPTER_END = {
        "E": "Chapter 1 Complete",   # Attack the Watchpoint
        "D": "Chapter 2 Complete",   # Ocean Crossing
        "A": "Chapter 3 Complete",   # Destroy the Ice Worm
    }
    arena_count = world._arena_count()
    arena_tiers = ["F", "E", "D", "C", "B", "A", "S"]
    for tier in arena_tiers[:arena_count]:
        if tier in ARENA_UNLOCK_CHAPTER_END:
            rule = (lambda state, it=ARENA_UNLOCK_CHAPTER_END[tier]:
                    state.has(it, player))
        else:
            rule = (lambda state, m=ARENA_UNLOCK_MISSION[tier]:
                    state.can_reach(m, "Location", player))
        for name in _with_bands(active_names, f"Complete Arena {tier}"):
            set_rule(multiworld.get_location(name, player), rule)

    # Mercenary ranks: sequential only. Unlock timing is uncertain, so ranks
    # 11+ are excluded (filler only) rather than gated.
    rank_count = world._rank_count()
    RANK_EXCLUDE_FROM = 11
    for i in range(2, rank_count + 1):
        set_rule(
            multiworld.get_location(f"Reach Mercenary Rank {i}", player),
            lambda state, prev=i - 1: state.can_reach(
                f"Reach Mercenary Rank {prev}", "Location", player),
        )
    for i in range(RANK_EXCLUDE_FROM, rank_count + 1):
        for name in _with_bands(active_names, f"Reach Mercenary Rank {i}"):
            multiworld.get_location(name, player).progress_type = \
                LocationProgressType.EXCLUDED
