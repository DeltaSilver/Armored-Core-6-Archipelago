# AC6 Archipelago — definitive event-flag reference

Compiled from full discovery playthroughs of **all three NG cycles** (NG → NG+ →
NG++, including branch missions). "Cycled" flags reset every NG cycle and fire
again; "one-time" flags persist across cycles.

## Story completion counter — CYCLED (the per-mission story checks)
One flag ticks up per mission cleared in that chapter, in play order (so a given
flag isn't a fixed mission — it's "the Nth mission cleared this chapter"). Counts
vary per chapter. These are the flags the mod uses as story checks.

| Chapter | Flags | Count |
|---|---|---|
| 1 | 3400–3409, 3450–3453 | 14 |
| 2 | 3410–3412 | 3 |
| 3 | 3420–3429, 3460–3466 | 17 |
| 4 | 3430–3436 | 7 |
| 5 | 3440–3443, 3447 | 5 |

**Branch-reserved (didn't fire in the 3 routes tested, but ARE referenced in the
mission-select controller — kept as checks for other branch choices):**
3413–3419 (Ch2), 3437–3439 (Ch4), 3444–3446 (Ch5). No soft-lock risk: every
part item is `useful`-class, only "Victory" is progression and it's placed
locally. So a check that never fires for a given player just strands a useful
part — harmless. Total story checks = 59. `3409` also triggers the
first-garage-visit grant gate.

## Key-mission milestones — ONE-TIME (persist across cycles)
Fire once on specific story missions (branch/route dependent).

| Flag | Mission/milestone |
|---|---|
| 6200 | Chapter 1 Submission |
| 6210 | Mining Ship & Dam (Ch1) |
| 6220 | Over the Wall (Ch1) |
| 6230 | Ch2 boundary |
| 6240 | Continental Crust (Ch3) |
| 6245 | Old Spaceport (Ch3) |
| 6250 | Defeat Ice Worm (Ch3) — confirmed |
| 6275 | Coral Export Denial (Ch3, NG+) |
| 6280 | Coral Convergence (Ch4) |
| 6260 | Prison Break / MIA (Ch5) — confirmed |

## Endings — GOAL flags (one per cycle, persist)
- `6000` = Ending A (Fires of Ibis) — NG
- `6001` = Ending B (Liberator of Rubicon) — NG+
- `6002` = Ending C (Alea Iacta Est / Allmind) — NG++

## Other confirmed
- Merc ranks: `6401`–`6417` (one-time, score-based; reached 6409 in testing).
- Arena: `6050`–`6056` (one-time).
- NG++ Allmind-route flags: `6015`–`6018` (cycled, NG++ only).
- Early/uncategorised: `6461`–`6464` (fire on first Ch1 mission, persist — purpose
  unconfirmed; not used as checks).

## NOT used as checks (noise / non-per-mission)
- `3139`, `3200` — menu / mission-select toggles (flip SET→clear constantly).
- `3122`–`3158` — mission *availability/unlock* flags (batch-set when missions open).
- `3000/3002/3004/3006/3008` — single-active chapter marker.
- `3220`, `3500`, `3210` — misc/transient.

## Branching note
Mission availability branches on player choices (e.g. Ayre vs Liberator routes),
so any single playthrough fires only a subset of the union above. The mod
registers the whole union; un-taken branches simply don't fire (no soft-lock,
since AC6 has no item-gated progression).
