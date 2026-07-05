# Armored Core 6 Archipelago

An [Armored Core VI: Fires of Rubicon](https://store.steampowered.com/app/1888160/)
integration for the [Archipelago](https://archipelago.gg/) multiworld randomizer.

Story missions, arena fights, and mercenary-rank milestones send Archipelago
checks; the AC parts you receive are shuffled across the whole multiworld.


## Requirements

- Armored Core VI: Fires of Rubicon (Steam)
- [ModEngine2](https://github.com/soulsmods/ModEngine2/releases) - loads the mod DLL
- [Archipelago](https://archipelago.gg/)

## Components

| Component | Path | Description |
|-----------|------|-------------|
| AP World | [`worlds/armored_core_6/`](worlds/armored_core_6) | Archipelago world definition - install into your AP `worlds/` folder or package as `.apworld` |
| Mod folder | [`mods/ac6ap/`](mods/ac6ap) | The files that go into your ModEngine2 `mod/` directory (`ac6ap.cfg`, the built `ac6ap.dll`, `regulation.bin`) |

## Setup

> **Use a throwaway save.** This mod is meant for a dedicated playthrough. Back up
> `AC60000.sl2` in `C:\Users\<User>\AppData\Roaming\ArmoredCore6\<your-steam-id>\`
> and move it aside so the game makes a new save; restore it afterwards.

1. **Set up ModEngine2** and confirm it launches the game normally first.
2. **Copy the [mod](https://github.com/DeltaSilver/Armored-Core-6-Archipelago/releases) files** into your ModEngine2 `mod` folder:
```
mod/
├── ac6ap.dll
├── ac6ap.cfg
├── regulation.bin
├── event/
└── msg/
```
4. **Register the DLL** in `config_armoredcore6.toml`:
   ```toml
   external_dlls = ["mod/ac6ap.dll"]
   ```
5. **Launch through ME2** with `launchmod_armoredcore6.bat` - never from Steam directly.

`slot` in `ac6ap.cfg` must match the `name` in your YAML exactly. A log file
(`ac6ap_log.txt`) appears in the mod folder - check it if the mod doesn't connect.

**Connecting in-game:** press **F8** to open the Archipelago settings window, where
you can set Host / Port / Slot / Password and hit **Connect** alternatively, you can edit `ac6ap.cfg`. 
It saves your entries back to `ac6ap.cfg` and reconnects without restarting.

---

<details>
<summary>Locations (Checks)</summary>

- **Story missions** - one check per mission cleared, named after the mission
  (Operation Wallclimber, Destroy the Ice Worm, …). Internally these are
  per-chapter completion counters, so a name is the mission that fills that slot
  on the normal route; branch decision points are shown as "A/B" (e.g. Eliminate
  the Enforcement Squads/Destroy the Special Forces Craft).
- **Mercenary ranks** - reach each merc rank 1-17.
- **Arena** - clear Arena ranks F → S, plus the three simulators.
- **Shop** - optional purchase checks (off by default).

A single run follows one route, so branch-reserved missions that never fire are
trimmed from the seed. See [`FLAG_REFERENCE.md`](FLAG_REFERENCE.md) for the exact
flags.

</details>

<details>
<summary>Items</summary>

The item pool is the AC parts catalogue (~314 parts), shuffled across the
multiworld. Filler is `COAM x50000`. Received parts appear in the assembly menu
(back out and re-enter if one doesn't show immediately).

`mission_reward_multiplier` (1×-4×) controls how many item checks each
objective awards.

</details>

<details>
<summary>Options</summary>

| Option | Values | Description |
|--------|--------|-------------|
| `mission_reward_multiplier` | 1-4 | Item checks awarded per mission / arena / rank objective |
| `shop_checks` | true / false | Add shop purchases as check locations (leftover parts spill into the shop) |
| `mercenary_rank_checks` | 0-17 | How many mercenary rank checks to include |
| `arena_checks` | 0-10 | How many arena checks to include (F → S, then the three simulators) |

Only single-playthrough runs are supported right now. NG+/NG++ multi-cycle
modes are disabled pending further testing.

</details>

---

## Notes / known limits

- An on-screen feed (top-left) shows items you receive,
  checks you complete, and items you send to other players.
  It is a separate top-most overlay window, so it shows in
  **borderless/windowed** mode, not exclusive fullscreen.
  
- Some missions and training will still grant items.
  
- Crashing during a mission removes items received during that mission.

- `AC60000.sl2` is the only way to recover progress - always back up first.
