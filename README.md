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
| DLL source | [`build/`](build) | C++ source for `ac6ap.dll` and how to compile it - see [`build/BUILDING.md`](build/BUILDING.md) |

For how the DLL talks to the game and the server, see [`CODE_REFERENCE.md`](CODE_REFERENCE.md).
For the full event-flag map, see [`FLAG_REFERENCE.md`](FLAG_REFERENCE.md).

## Setup

> **Use a throwaway save.** This mod is meant for a dedicated playthrough. Back up
> `AC60000.sl2` in `C:\Users\<User>\AppData\Roaming\ArmoredCore6\<your-steam-id>\`
> and move it aside so the game makes a new save; restore it afterwards.

1. **Set up ModEngine2** and confirm it launches the game normally first.
2. **Build `ac6ap.dll`** from [`build/`](build) (see [`build/BUILDING.md`](build/BUILDING.md)), or grab it from the mod release.
3. **Copy the mod files** into your ModEngine2 `mod/` folder:
   - `ac6ap.dll` (built)
   - `ac6ap.cfg` (from [`mods/ac6ap/`](mods/ac6ap) - edit with your room details)
   - `regulation.bin` (from the mod release)
4. **Register the DLL** in `config_armoredcore6.toml`:
   ```toml
   external_dlls = ["mod/ac6ap.dll"]
   ```
5. **Launch through ME2** with `launchmod_armoredcore6.bat` - never from Steam directly.

`slot` in `ac6ap.cfg` must match the `name` in your YAML exactly. A log file
(`ac6ap_log.txt`) appears in the mod folder - check it if the mod doesn't connect.

**Connecting in-game:** press **F8** to open the Archipelago settings window, where
you can set Host / Port / Slot / Password and hit **Connect** without editing the
cfg or restarting. It saves your entries back to `ac6ap.cfg` and reconnects live,
so you can also fix a typo or switch rooms mid-session. (Borderless/windowed mode;
it takes keyboard focus while open.)

---

<details>
<summary>Run Modes</summary>

AC6's story flags reset on every New Game cycle, so the same missions can be
replayed in NG+ and NG++. `run_mode` in your YAML chooses how the run is scoped:

| Mode | Cycles played | Goal | Story checks |
|------|---------------|------|--------------|
| `single` | NG | reach any one ending | NG only |
| `ng_plus_run` | NG → NG+ | reach both of the first two endings | each mission once |
| `ng_plus_run_cycled` | NG → NG+ | both endings | each mission, **per cycle** (~2×) |
| `full_run` | NG → NG+ → NG++ | all three endings | each mission once |
| `full_run_cycled` | NG → NG+ → NG++ | all three endings | each mission, **per cycle** (~3×) |

The `*_cycled` modes give each playthrough cycle its own independent set of
mission checks; the others fire each check once and use NG+/NG++ only to reach
the later endings.

</details>

<details>
<summary>Locations (Checks)</summary>

- **Story missions** - one check per mission cleared, named after the mission
  (Operation Wallclimber, Destroy the Ice Worm, …). Internally these are
  per-chapter completion counters, so a name is the mission that fills that slot
  on the normal route; branch decision points are shown as "A/B" (e.g. Eliminate
  the Enforcement Squads/Destroy the Special Forces Craft).
- **Key missions** - 10 named story-milestone checks (Mining Ship & Dam, Defeat
  Ice Worm, Prison Break, …).
- **Mercenary ranks** - reach each merc rank 1-17.
- **Arena** - clear Arena ranks F → S.
- **Archive logs** - optional collectible checks (experimental; off by default).

A single run follows one route, so branch-reserved missions that never fire are
trimmed from `single` seeds; multi-cycle runs revisit them across cycles. See
[`FLAG_REFERENCE.md`](FLAG_REFERENCE.md) for the exact flags.

</details>

<details>
<summary>Items</summary>

The item pool is the AC parts catalogue (~314 parts), shuffled across the
multiworld. Filler is `COAM x10000`. Received parts appear in the assembly menu
(back out and re-enter if one doesn't show immediately).

The pool also includes small auto-managed `Chapter 2..5 Access` (and, on
multi-cycle modes, `NG+`/`NG++ Access`) progression items. These are logic gates
only - the mod never grants anything for them in-game. They make AC6 an ordered
early-to-late game in the multiworld's logic, so Chapter 1 is "early" and the
finale is "late". That keeps other games' early items in AC6's early chapters and
their late items out of your endgame. You never configure them.

`mission_reward_multiplier` (1×-4×) controls how many item checks each
objective awards.

</details>

<details>
<summary>Options</summary>

| Option | Values | Description |
|--------|--------|-------------|
| `run_mode` | single / ng_plus_run / ng_plus_run_cycled / full_run / full_run_cycled | How many NG cycles the run spans and whether each cycle has its own checks |
| `mission_reward_multiplier` | 1-4 | Item checks awarded per objective |
| `archive_logs` | off / on | Add the archive-log collectibles as checks (experimental) |

</details>

<details>
<summary>Dev / debug toggles (ac6ap.cfg)</summary>

These live in `ac6ap.cfg`, are off by default, and don't affect normal play:

- `discover=1` - log every event flag that flips to `ac6ap_discovery.txt`
  instead of connecting to Archipelago. Used to map missions to their trigger
  flags in a single playthrough.
- `grant_all_parts=1` - enables an **F7** hotkey: press F7 at the garage to add
  every AC part to your inventory.

</details>

---

## Notes / known limits

- An on-screen feed (top-left) shows items you receive (with who sent them),
  checks you complete, and items you send to other players (named via the AP
  DataPackage). It is a separate top-most overlay window, so it shows in
  **borderless/windowed** mode (the default), not exclusive fullscreen; set
  `overlay=0` in `ac6ap.cfg` to disable it. The AP text client remains the full
  record.
- Shop-purchase checks are not implemented. Checks come from story, key missions,
  arena, and mercenary ranks (plus optional archive logs).
- Crashing during a mission removes items received during that mission.
- `AC60000.sl2` is the only way to recover progress - always back up first.
