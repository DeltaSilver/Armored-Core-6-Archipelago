from worlds.AutoWorld import World, WebWorld
from BaseClasses import (
    Region, Location, Item, ItemClassification, Tutorial
)
from .items import (
    AC6ItemData, ITEM_TABLE, PART_NAMES, BASE_ID,
    VICTORY_OFFSET, COAM_OFFSET, CHAPTER_COMPLETE_ITEMS,
)

_SPECIAL_ITEMS = {"COAM x50000", "AC6 Victory", *CHAPTER_COMPLETE_ITEMS}
PART_COUNT = sum(1 for n in ITEM_TABLE if n not in _SPECIAL_ITEMS)

from .locations import (
    AC6LocationData, LOCATION_TABLE, BASE_LOC_ID,
    make_multiplier_locations, all_multiplier_locations,
    make_shop_locations, all_shop_locations,
    SHOP_LOC_BASE, SHOP_FLAG_BASE, SHOP_SLOTS,
    SHOP_CHAPTERS, SHOP_ITEMS_PER_CH, SHOP_ITEMS_PER_BATCH,
    SHOP_BATCHES_PER_CH, SHOP_GATE_FLAGS, SHOP_BATCH_FLAG_BASE,
    chapter_complete_locations,
)
from .options import AC6Options
from .rules import set_rules
from typing import Dict, Any, List


EARLY_ITEMS = ["VE-60SNA (L)"]


def _merge_unique(*tables: Dict[str, AC6LocationData]) -> Dict[str, AC6LocationData]:
    """Merge location tables, failing on duplicate names or IDs (a silent
    overwrite drops IDs from the data package, which breaks strict clients)."""
    out: Dict[str, AC6LocationData] = {}
    by_code: Dict[int, str] = {}
    for table in tables:
        for name, d in table.items():
            if name in out and out[name].code != d.code:
                raise Exception(f"AC6: location name {name!r} has two IDs "
                                f"({out[name].code}, {d.code})")
            if by_code.get(d.code, name) != name:
                raise Exception(f"AC6: location ID {d.code} used by "
                                f"{by_code[d.code]!r} and {name!r}")
            out[name] = d
            by_code[d.code] = name
    return out


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
    Progress through missions, complete arena challenges, and reach the
    ending to complete your goal. AC parts are shuffled throughout the
    multiworld. Base game (first playthrough) only - no NG+ / NG++.
    """

    game = "Armored Core VI"
    web = AC6Web()
    topology_present = True
    options_dataclass = AC6Options
    options: AC6Options  # type: ignore

    item_name_to_id: Dict[str, int] = {
        name: data.code for name, data in ITEM_TABLE.items()
    }

    location_name_to_id: Dict[str, int] = {
        name: data.code
        for name, data in _merge_unique(
            LOCATION_TABLE,
            all_multiplier_locations(),
            all_shop_locations(),
            chapter_complete_locations(),
        ).items()
    }

    # -- Option helpers --------------------------------------------------

    def _shop_checks_on(self) -> bool:
        return bool(self.options.shop_checks.value)

    def _compute_batches_per_ch(self) -> int:
        if not self._shop_checks_on():
            return 0
        gap = PART_COUNT - len(self._mission_locations())
        if gap <= 0:
            return 0
        return min((gap + 99) // 100, SHOP_BATCHES_PER_CH)

    def _rank_count(self) -> int:
        return int(self.options.mercenary_rank_checks.value)

    def _arena_count(self) -> int:
        return int(self.options.arena_checks.value)

    def _multiplier(self) -> int:
        return int(self.options.mission_reward_multiplier.value)

    def _build_base_table(self, rank_count: int,
                           arena_count: int) -> Dict[str, AC6LocationData]:
        rank_seen = arena_seen = 0
        base: Dict[str, AC6LocationData] = {}
        for name, d in LOCATION_TABLE.items():
            if d.region == "Ranks":
                if rank_seen < rank_count:
                    base[name] = d
                    rank_seen += 1
            elif d.region == "Arena":
                if arena_seen < arena_count:
                    base[name] = d
                    arena_seen += 1
            else:
                base[name] = d
        return base

    def _mission_locations(self) -> Dict[str, AC6LocationData]:
        base = self._build_base_table(self._rank_count(), self._arena_count())
        mult = self._multiplier()
        if mult > 1:
            base.update(make_multiplier_locations(mult, base))
        return base

    def _active_locations(self) -> Dict[str, AC6LocationData]:
        locs = self._mission_locations()
        bpc = self._compute_batches_per_ch()
        if bpc > 0:
            locs.update(make_shop_locations(bpc))
        return locs

    # -- AP world interface ----------------------------------------------

    def generate_early(self) -> None:
        for item_name in EARLY_ITEMS:
            if item_name in self.item_name_to_id:
                self.multiworld.early_items[self.player][item_name] = 1

    def create_regions(self) -> None:
        region_names = (["Menu", "Arena", "Ranks"]
                        + [f"Shop Ch{c}" for c in range(1, SHOP_CHAPTERS + 1)]
                        + [f"Chapter {ch}" for ch in range(1, 6)])

        regions: Dict[str, Region] = {}
        for name in region_names:
            r = Region(name, self.player, self.multiworld)
            regions[name] = r
            self.multiworld.regions.append(r)

        for loc_name, loc_data in self._active_locations().items():
            region = regions[loc_data.region]
            location = AC6Location(self.player, loc_name, loc_data.code, region)
            region.locations.append(location)

        # Locked Victory event at the end of the game
        reg = regions["Chapter 5"]
        loc = AC6Location(self.player, "Reach an Ending", None, reg)
        loc.place_locked_item(
            AC6Item("Victory", ItemClassification.progression, None, self.player)
        )
        reg.locations.append(loc)

        # Chapter N Complete: real location (sent by the DLL) with a locked item
        # of the same name; gates the next chapter. Not in the item pool count.
        for name, data in chapter_complete_locations().items():
            reg = regions[data.region]
            loc = AC6Location(self.player, name, data.code, reg)
            loc.place_locked_item(self.create_item(name))
            reg.locations.append(loc)

        # Region connections
        menu = regions["Menu"]
        menu.connect(regions["Chapter 1"], "Menu -> Chapter 1")
        menu.connect(regions["Arena"], "Menu -> Arena")
        menu.connect(regions["Ranks"], "Menu -> Ranks")
        for c in range(1, SHOP_CHAPTERS + 1):
            menu.connect(regions[f"Shop Ch{c}"], f"Menu -> Shop Ch{c}")
        for ch in range(1, 5):
            regions[f"Chapter {ch}"].connect(
                regions[f"Chapter {ch + 1}"], f"Chapter {ch} -> Chapter {ch + 1}")

    def create_items(self) -> None:
        self._cached_bpc = self._compute_batches_per_ch()
        location_count = len(self._active_locations())
        items: List[AC6Item] = []

        # Part pool (shuffled; EARLY_ITEMS pushed to front)
        pool_names: List[str] = [
            name for name in ITEM_TABLE if name not in _SPECIAL_ITEMS
        ]
        self.random.shuffle(pool_names)
        for prio in reversed(EARLY_ITEMS):
            if prio in pool_names:
                pool_names.remove(prio)
                pool_names.insert(0, prio)

        for i in range(location_count):
            name = pool_names[i] if i < len(pool_names) else "COAM x50000"
            items.append(self.create_item(name))

        self.multiworld.itempool += items

    def create_item(self, name: str) -> AC6Item:
        data = ITEM_TABLE[name]
        return AC6Item(name, data.classification, data.code, self.player)

    def set_rules(self) -> None:
        set_rules(self, self.multiworld, self.player)

    def get_filler_item_name(self) -> str:
        return "COAM x50000"

    def generate_basic(self) -> None:
        self.multiworld.completion_condition[self.player] = (
            lambda state: state.has("Victory", self.player)
        )

    def fill_slot_data(self) -> Dict[str, Any]:
        slot_data: Dict[str, Any] = {
            "game_version": "1.0",
            "mission_reward_multiplier": self._multiplier(),
            "death_link": False,  # DeathLink option disabled for v0.2.1
            "batches_per_ch": getattr(self, "_cached_bpc", self._compute_batches_per_ch()),
        }

        bpc = getattr(self, "_cached_bpc", self._compute_batches_per_ch())
        if bpc > 0:
            player_name = self.multiworld.get_player_name(self.player)
            slots = []
            items_per_ch = bpc * SHOP_ITEMS_PER_BATCH
            for ch in range(SHOP_CHAPTERS):
                for i in range(items_per_ch):
                    slot_0 = ch * SHOP_ITEMS_PER_CH + i
                    loc_name = f"[Ch{ch + 1}] Shop Item {i + 1}"
                    location = self.multiworld.get_location(loc_name, self.player)
                    item_name = (location.item.name
                                 if location.item is not None else "Unknown")
                    slots.append({
                        "slot":         slot_0,
                        "chapter":      ch + 1,
                        "display_name": f"{player_name}'s {item_name}",
                        "ap_loc_id":    SHOP_LOC_BASE + slot_0,
                        "flag_id":      SHOP_FLAG_BASE + slot_0,
                    })
            slot_data["shop_slots"] = slots

        return slot_data
