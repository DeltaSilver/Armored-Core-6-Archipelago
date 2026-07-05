# Armored Core 6 Archipelago - Code Reference

How the mod is put together: what the DLL reads from the running game, how it
talks to the Archipelago server, and how the AP world maps to it. Intended as a
quick orientation so you don't have to read every source file.

---

## Architecture

Two halves talk to each other over a standard Archipelago connection:

```
  Armored Core VI (armoredcore6.exe)
        |  injected by ModEngine2
        v
  ac6ap.dll  --- reads event flags, grants parts
        |  WebSocket (ws:// or wss://)
        v
  Archipelago server  <--->  worlds/armored_core_6 (AP world: items, locations, rules)
```

- **`ac6ap.dll`** (source in [`build/`](build)) runs inside the game. It watches
  in-game **event flags** to detect progress and sends location checks, and it
  **grants AC parts** the player receives by calling the game's own add-item
  function. It does not modify game code - it reads memory and calls one function.
  It also draws a small **on-screen overlay** (`overlay.cpp`) listing received
  items and completed checks - a separate layered top-most GDI window, *not* a
  renderer hook, so it cannot crash the game or clash with ModEngine2 (shows in
  borderless/windowed mode only).
- **`worlds/armored_core_6`** is the Python AP world. It defines the item pool,
  the location IDs, the regions/rules, and the slot options. It never touches the
  game directly.
- **`regulation.bin`** (shipped with the release, not in this repo) is the modded
  param file that removes parts from the normal economy so they arrive from AP.

---

## What the DLL pulls from the game

All of this is set up in `dllmain.cpp` `MainThread` at launch.

| Thing | How it's found | Used for |
|-------|----------------|----------|
| `CSEventFlagMan` pointer | AOB scan `48 8B 35 ? ? ? ? 83 F8 FF 0F 44 C1`, resolved as a RIP-relative pointer | Base for all event-flag reads |
| Save "divisor" | `*(eventFlagMan + 0x1C)` | Non-zero only when a save is live; gates all reads/grants |
| `AddItem(int* idPtr, int qty)` | AOB scan `?? 89 ?? ?? 8B ?? ?? 89 ?? ?? 8B D7 ?? 8D`, the `CALL` rel32 at offset 16 | Granting received parts |

### Event flags

`flagwriter.cpp` `ReadEventFlag(eventFlagMan, divisor, flagId)` walks AC6's
event-flag structure (the standard FromSoft red-black-tree-of-blocks layout):
it splits the flag into `category = flagId / divisor` and a bit index, finds the
category's block, and tests the bit. `WriteEventFlag` is the mirror (unused at
runtime). This is the single primitive the whole mod is built on.

The flag the mod watches are listed in `build/locations.h` (`g_locations`) and
documented in [`FLAG_REFERENCE.md`](FLAG_REFERENCE.md). Categories:

- **`3000`-`3999` - story / mission flags (cycled).** Positional story
  counters: clearing one mission can tick 2-3 of them at once, so they are NOT
  1:1 with missions. The world keeps only one counter per mission and drops the
  redundant co-firing ticks (`COLLAPSED_FLAGS`) plus the never-firing branch
  flags (`BRANCH_RESERVED_FLAGS`), leaving a clean one-check-per-mission set with
  names lined up from the discovery playthroughs (branch decisions joined "A/B").
  These **reset every NG cycle**, so the DLL treats them specially (see *NG
  cycles*). `AC6_IsCycledFlag()` is the test. `3409` (intro/garage) is collapsed
  out as a check but the DLL still reads it directly for the first-garage grant
  gate.
- **`6050`-`6056` arena, `6200`-`6280` key missions, `6401`-`6417` merc ranks -
  one-time.** They persist across NG cycles and fire once.
- **`6000`/`6001`/`6002` - endings A / B / C.** Used for goal detection; they
  accumulate (persist), so "N endings reached" = count of these that are set.

### Granting items

`dllmain.cpp` `GrantItem(itemId, qty)` allocates a small zeroed buffer, writes
the part ID at offset 0, and calls `AddItem`, wrapped in `__try/__except`. Grants
are queued (`QueueGrant`) and drained one at a time by `GrantDrainThread`, gated
on a live save and the first-garage-visit flag (`3409`). Part IDs ↔ names live in
`partnames.h`; `allparts.h` is the full list used by the F7 unlock hotkey.

---

## Archipelago protocol (apclient.cpp)

A minimal hand-rolled client over `easywsclient` (JSON parsed by string scanning):

- On `RoomInfo` → sends `Connect` (`game = "Armored Core VI"`, `items_handling = 7`,
  `slot_data = true`) and `GetDataPackage` for the room's games.
- On `Connected` → captures slot number, reads `run_mode` from `slot_data`, parses
  the players list (slot → name) and `slot_info` (slot → game), loads the persisted
  receive-count and NG cycle.
- On `DataPackage` → parses each game's `item_name_to_id` into game → id → name (a
  small `SkipString`/`MatchBrace` scanner handles names with quotes/colons/braces).
  Used to name items sent to other players in the overlay.
- On `ReceivedItems` → grants any items past the persisted count (so reconnects,
  which resend from index 0, never double-grant); overlay shows "from \<player\>".
- On `PrintJSON` `ItemSend` where we are the finder → overlay shows
  "Sent \<item\> to \<player\>" (item resolved via the receiver's game).
- Sends `LocationChecks` for checks; `StatusUpdate` (goal) when the run's required
  number of endings is reached.

### Location ID scheme

Must match `worlds/armored_core_6/locations.py` exactly:

```
location_id = BASE_LOC_ID + cycle*CYCLE_OFFSET + band*MULTIPLIER_OFFSET + flagId
              7700000        ×2,000,000          ×500,000
```

- **flagId** - the in-game event flag (`AC6_BASE_ID = 7700000` in the DLL).
- **band** - multiplier reward copy (0-3). The DLL always sends every band; the
  server keeps only the ones the seed generated, so the DLL never needs to know
  the multiplier.
- **cycle** - NG cycle (0/1/2). Only applied to cycled (story) flags, so each NG
  cycle's mission checks get distinct IDs.

### slot_data the DLL reads

| Key | Meaning |
|-----|---------|
| `run_mode` | 0 single, 1 ng_plus_run, 2 ng_plus_run_cycled, 3 full_run, 4 full_run_cycled |

`CyclesForMode()` derives cycle count (1/2/3) and `IsCycledMode()` (modes 2, 4)
from it. The goal fires once `>=` that many of `{6000,6001,6002}` are set.

---

## NG cycles

Because story flags reset each cycle, the same flag fires in NG, NG+, and NG++
for different missions. The DLL keeps them distinct:

1. **Detection** - `flagwatcher.cpp` watches already-sent story flags; when ≥ 8
   of them clear in one poll (the NG+ wipe), it calls `APClient_AdvanceCycle()`
   and forgets the story flags so they re-send as the next cycle's checks.
2. **Persistence** - the cycle is saved to `ac6ap_cycle_<seed>_<slot>.txt` and
   reloaded on connect, so a restart mid-NG++ stays on the right cycle.
3. **Offset** - cycled checks add `cycle * AC6_CYCLE_OFFSET` to the location ID.

Only the `*_cycled` run modes advance the cycle; the others stay at cycle 0 and
let NG+/NG++ re-fires dedupe server-side.

### Cycle gating (multiworld balance)

Within a cycle AC6 has no item-based gates, so on its own the region chain would
leave every NG+/NG++ check reachable from logic sphere 0. In a multiworld that is
a problem: the solver could place another game's early-critical progression
behind your NG++ finale. To prevent that, each extra cycle a run mode uses adds a
progression item - `NG+ Access` and `NG++ Access` - and `rules.py` gates the
cycle-transition entrances on them, so later-cycle checks are genuinely later in
logic. These two items use sentinel offsets (`BASE_ID + 2` / `+ 3`); the DLL
recognises them in `OnItemReceived` and does **not** grant anything in-game (they
exist only as logic gates). They are auto-managed - players never set them.

The same idea runs *within* a cycle: `Chapter 2..5 Access` (sentinel offsets
`BASE_ID + 4..7`) gate each chapter, so Chapter 1 (plus the first arena/rank
checks) is logic sphere 0 and the finale is the deepest sphere. This ordering is
what lets the multiworld place other games' **early** items in AC6's early
chapters and keep their late items out of your endgame. All passes are dropped by
the DLL the same way; only flag IDs are sent for checks, so the per-mission
naming is a Python-side concern with no DLL impact.

---

## Files the DLL writes (mod folder)

| File | Contents |
|------|----------|
| `ac6ap_log.txt` | Timestamped diagnostic log (overwritten each launch) |
| `ac6ap_checks.txt` | Every check sent, with its mission name, cycle, and location ID (append mode, survives restarts). For confirming a mission fired the right check / reporting mismatches. |
| `ac6ap_recv_<seed>_<slot>.txt` | Count of received items already granted |
| `ac6ap_cycle_<seed>_<slot>.txt` | Current NG cycle (0/1/2) |
| `ac6ap_discovery.txt` | Flag-flip log, only when `discover=1` (append mode) |

---

## Config (ac6ap.cfg)

Read by `LoadConfig`; a default is written if missing. Keys: `host`, `port`,
`slot`, `password`, plus the dev toggles `discover`, `discover_ranges`,
`grant_all_parts`. See [`mods/ac6ap/ac6ap.cfg`](mods/ac6ap/ac6ap.cfg).

### Discovery mode

`discover=1` skips Archipelago entirely and runs `FlagWatcher_StartDiscovery`,
which scans the configured ranges every poll and logs each flag that flips to
`ac6ap_discovery.txt`. This is how the flag map in
[`FLAG_REFERENCE.md`](FLAG_REFERENCE.md) was produced; one instrumented
playthrough per route reveals which flags each mission sets.
