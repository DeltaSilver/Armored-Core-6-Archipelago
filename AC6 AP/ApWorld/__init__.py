from worlds.AutoWorld import World, WebWorld
from BaseClasses import (
    Region, Location, Item, ItemClassification, Tutorial
)
from .items import (
    AC6ItemData, ITEM_TABLE, PART_NAMES, BASE_ID,
    VICTORY_OFFSET, COAM_OFFSET,
)
from .locations import (
    AC6LocationData, LOCATION_TABLE, ARCHIVE_LOG_LOCATIONS, BASE_LOC_ID,
    make_multiplier_locations, all_multiplier_locations,
    add_cycles, CYCLE_NAMES, NUM_CYCLES,
)
from .options import AC6Options
from .rules import set_rules
from typing import Dict, Any, List


# Items that must appear in sphere 1 so players can survive early.
EARLY_ITEMS = []


class AC6Location(Location):
    game = "Armored Core VI"


class AC6Item(Item):
    game = "Armored Core VI"


class AC6Web(WebWorld):
    theme = "dirt"
    tutorials: List[Tutorial] = []


class ArmoredCore6World(World):
    """
    Armored Core VI: Fires of Rubicon -- Archipelago.
    Progress through missions, complete arena challenges, and reach any
    ending to complete your goal. AC parts are shuffled throughout the
    multiworld.
    """

    game = "Armored Core VI"
    web = AC6Web()
    topology_present = True
    options_dataclass = AC6Options
    options: AC6Options  # type: ignore

    item_name_to_id: Dict[str, int] = {
        name: data.code for name, data in ITEM_TABLE.items()
    }

    # All possible locations need IDs registered, even ones a given seed
    location_name_to_id: Dict[str, int] = {
        name: data.code
        for name, data in add_cycles({
            **LOCATION_TABLE,
            **all_multiplier_locations(),
            **ARCHIVE_LOG_LOCATIONS,
        }).items()
    }

    # -- Option helpers --------------------------------------------------

    def _archive_logs_on(self) -> bool:
        return bool(self.options.archive_logs.value)

    def _multiplier(self) -> int:
        return int(self.options.mission_reward_multiplier.value)

    # run_mode: 0 single, 1 ng_plus_run, 2 ng_plus_run_cycled,
    #           3 full_run, 4 full_run_cycled
    _CYCLES_PER_MODE = {0: 1, 1: 2, 2: 2, 3: 3, 4: 3}
    _CYCLED_MODES = {2, 4}

    def _run_mode(self) -> int:
        return int(self.options.run_mode.value)

    def _num_cycles(self) -> int:
        # single = 1 (NG), ng_plus_* = 2 (NG->NG+), full_* = 3 (NG->NG+->NG++).
        return self._CYCLES_PER_MODE[self._run_mode()]

    def _dup_checks(self) -> bool:
        # The "_cycled" modes give each cycle its own copy of the story checks.
        return self._run_mode() in self._CYCLED_MODES

    def _active_locations(self) -> Dict[str, AC6LocationData]:
        locs = dict(LOCATION_TABLE)
        mult = self._multiplier()
        if mult > 1:
            locs.update(make_multiplier_locations(mult))
        if self._archive_logs_on():
            locs.update(ARCHIVE_LOG_LOCATIONS)
        # Cycled modes duplicate the story checks across this run's cycle count.
        if self._dup_checks():
            return add_cycles(locs, self._num_cycles())
        return locs

    # -- AP world interface ----------------------------------------------

    def generate_early(self) -> None:
        for item_name in EARLY_ITEMS:
            if item_name in self.item_name_to_id:
                self.multiworld.early_items[self.player][item_name] = 1

    def create_regions(self) -> None:
        # One set of Chapter 1-5 regions per NG cycle, plus shared Arena/Ranks.
        # Cycle 0 (NG) regions are unprefixed ("Chapter 1"); NG+/NG++ are
        # prefixed ("NG+ Chapter 1", ...).
        def chapter_region(cycle: int, chapter: int) -> str:
            prefix = "" if cycle == 0 else f"{CYCLE_NAMES[cycle]} "
            return f"{prefix}Chapter {chapter}"

        ncyc = self._num_cycles()   # 1 for single, NUM_CYCLES for full runs

        region_names = ["Menu", "Arena", "Ranks"]
        for cyc in range(ncyc):
            for ch in range(1, 6):
                region_names.append(chapter_region(cyc, ch))

        regions: Dict[str, Region] = {}
        for name in region_names:
            r = Region(name, self.player, self.multiworld)
            regions[name] = r
            self.multiworld.regions.append(r)

        # Place all active locations into their regions.
        for loc_name, loc_data in self._active_locations().items():
            region = regions[loc_data.region]
            location = AC6Location(
                self.player, loc_name, loc_data.code, region
            )
            region.locations.append(location)

        # Goal event locations -- locked Victory items, no AP item ID. Each
        # route's ending happens in its own NG cycle (A=NG, B=NG+, C=NG++); a
        # single run only has the first ending. The completion condition uses
        # the Victory count (see generate_basic).
        route_names = ["Route A Clear", "Route B Clear", "Route C Clear"]
        for cyc in range(ncyc):
            reg = regions[chapter_region(cyc, 5)]
            loc = AC6Location(self.player, route_names[cyc], None, reg)
            loc.place_locked_item(
                AC6Item("Victory", ItemClassification.progression,
                        None, self.player)
            )
            reg.locations.append(loc)

        # Connect regions. Within a cycle the chapters chain 1->2->...->5; the
        # last chapter of a cycle leads into the next cycle's first chapter.
        # No item-based hard gates exist in AC6, so a linear chain is both
        # correct (everything stays reachable) and robust to per-chapter mission
        # counts (which vary and differ again in NG+/NG++).
        menu = regions["Menu"]
        menu.connect(regions[chapter_region(0, 1)], "Menu -> Chapter 1")
        menu.connect(regions["Arena"], "Menu -> Arena")
        menu.connect(regions["Ranks"], "Menu -> Ranks")
        for cyc in range(ncyc):
            for ch in range(1, 5):
                a = regions[chapter_region(cyc, ch)]
                b = regions[chapter_region(cyc, ch + 1)]
                a.connect(b, f"{chapter_region(cyc, ch)} -> {chapter_region(cyc, ch + 1)}")
            if cyc + 1 < ncyc:
                regions[chapter_region(cyc, 5)].connect(
                    regions[chapter_region(cyc + 1, 1)],
                    f"{chapter_region(cyc, 5)} -> {chapter_region(cyc + 1, 1)}")

    def create_items(self) -> None:
        location_count = len(self._active_locations())

        # Candidate part pool.
        pool_names: List[str] = [
            name for name in ITEM_TABLE
            if name not in ("COAM x10000", "AC6 Victory") # This will never occur as locations < items.
        ]

        # Shuffle with the seeded world RNG so the chosen subset varies
        # across all part types, deterministically per seed.
        self.random.shuffle(pool_names)

        # Guarantee priority/early items survive the slice.
        for prio in reversed(EARLY_ITEMS):
            if prio in pool_names:
                pool_names.remove(prio)
                pool_names.insert(0, prio)

        items: List[AC6Item] = []
        for i in range(location_count):
            name = pool_names[i] if i < len(pool_names) else "COAM x10000"
            items.append(self.create_item(name))

        self.multiworld.itempool += items

    def create_item(self, name: str) -> AC6Item:
        data = ITEM_TABLE[name]
        return AC6Item(name, data.classification, data.code, self.player)

    def set_rules(self) -> None:
        set_rules(self, self.multiworld, self.player)

    def get_filler_item_name(self) -> str:
        return "COAM x10000"

    def generate_basic(self) -> None:
        # Victory items needed = cycle count: single=1 (any ending),
        # ng_plus_*=2 (NG + NG+ endings), full_*=3 (all three endings).
        need = self._num_cycles()
        self.multiworld.completion_condition[self.player] = (
            lambda state, n=need: state.has("Victory", self.player, n)
        )

    def fill_slot_data(self) -> Dict[str, Any]:
        return {
            "game_version": "1.0",
            "archive_logs": self._archive_logs_on(),
            "mission_reward_multiplier": self._multiplier(),
            "run_mode": self._run_mode(),
            "death_link": False,
        }