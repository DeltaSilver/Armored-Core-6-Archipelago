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
        for name, data in {
            **LOCATION_TABLE,
            **ARCHIVE_LOG_LOCATIONS,
            **all_multiplier_locations(),
        }.items()
    }

    # -- Option helpers --------------------------------------------------

    def _archive_logs_on(self) -> bool:
        return bool(self.options.archive_logs.value)

    def _multiplier(self) -> int:
        return int(self.options.mission_reward_multiplier.value)

    def _active_locations(self) -> Dict[str, AC6LocationData]:
        locs = dict(LOCATION_TABLE)
        mult = self._multiplier()
        if mult > 1:
            locs.update(make_multiplier_locations(mult))
        if self._archive_logs_on():
            locs.update(ARCHIVE_LOG_LOCATIONS)
        return locs

    # -- AP world interface ----------------------------------------------

    def generate_early(self) -> None:
        for item_name in EARLY_ITEMS:
            if item_name in self.item_name_to_id:
                self.multiworld.early_items[self.player][item_name] = 1

    def create_regions(self) -> None:
        region_names = [
            "Menu", "Chapter 1", "Chapter 2", "Chapter 3",
            "Chapter 4", "Chapter 5", "Arena", "Ranks",
        ]
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

        # Goal event locations -- locked Victory items, no AP item ID.
        for route_name in ("Route A Clear", "Route B Clear", "Route C Clear"):
            loc = AC6Location(self.player, route_name, None, regions["Chapter 5"])
            loc.place_locked_item(
                AC6Item("Victory", ItemClassification.progression,
                        None, self.player)
            )
            regions["Chapter 5"].locations.append(loc)

        # Connect regions with named entrances (rules reference these).
        menu = regions["Menu"]
        for dest in ("Chapter 1", "Chapter 2", "Chapter 3",
                     "Chapter 4", "Chapter 5", "Arena", "Ranks"):
            menu.connect(regions[dest], f"Menu -> {dest}")

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
        goal_value = self.options.goal.value  # 0=any 1=A 2=B 3=C 4=all

        if goal_value == 0:
            self.multiworld.completion_condition[self.player] = (
                lambda state: state.has("Victory", self.player)
            )
        elif goal_value == 1:
            self.multiworld.completion_condition[self.player] = (
                lambda state: state.can_reach("Route A Clear", "Location", self.player)
            )
        elif goal_value == 2:
            self.multiworld.completion_condition[self.player] = (
                lambda state: state.can_reach("Route B Clear", "Location", self.player)
            )
        elif goal_value == 3:
            self.multiworld.completion_condition[self.player] = (
                lambda state: state.can_reach("Route C Clear", "Location", self.player)
            )
        else:
            self.multiworld.completion_condition[self.player] = (
                lambda state: state.has("Victory", self.player, 3)
            )

    def fill_slot_data(self) -> Dict[str, Any]:
        return {
            "game_version": "1.0",
            "archive_logs": self._archive_logs_on(),
            "mission_reward_multiplier": self._multiplier(),
            "goal": self.options.goal.value,
            "death_link": False,
        }