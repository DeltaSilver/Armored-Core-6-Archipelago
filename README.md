# Armored Core 6 Archipelago
An archipelago multiworld for Armored Core VI. Story, arena, and mercenary-rank
progress send Archipelago checks; the AC parts you receive are shuffled across
the multiworld.

## Save Data
This mod is meant for a **fresh save**. It doesn't create a separate save file
yet, so set your real save aside before playing and restore it afterwards.

1. **Back up your save.** Go to
   `C:\Users\<User>\AppData\Roaming\ArmoredCore6\<your-steam-id>\`
2. Move `AC60000.sl2` to another folder so the game makes a
   new save.

## Instructions

1. **Set up ModEngine2.** Download
   [ModEngine2](https://github.com/soulsmods/ModEngine2/releases) and confirm
   it launches the game normally before adding the mod.
2. **Copy the [mod](https://github.com/DeltaSilver/Armored-Core-6-Archipelago/releases) files** into your ModEngine2 `mod` folder:
   - `ac6ap.dll`
   - `ac6ap.cfg`
   - `regulation.bin`
3. **Register the DLL.** Open `config_armoredcore6.toml` and add `ac6ap.dll`
   to the `external_dlls` list:
```toml
   external_dlls = ["mod/ac6ap.dll"]
```
4. **Edit `ac6ap.cfg`** with your connection details:

5. **Launch through ME2.** Run `launchmod_armoredcore6.bat`. Do **not** launch
   from Steam directly, it must start through ModEngine2 for the mod to load.

## Important Notes
- `slot` must match the `name` in your YAML **exactly** (same spelling and
  capitalisation).
- Leave `password` blank if the room has no password.
- If you already use a modified `regulation.bin` for another mod, merge the
  changes rather than overwrite it.
- A log file (`ac6ap_log.txt`) appears in the mod folder — check it if the
  mod doesn't connect.
- Always back up before deleting anything. `AC60000.sl2` is the only
  way to recover your progress.
- Don't run the mod on your main save; Archipelago is meant for a dedicated
  playthrough.

## Notes / Known Limits
- Item-acquisition notifications don't pop on screen; check your AP text client to
  see what you received. Parts appear in the assembly menu (back out and
  re-enter the menu if you don't see one immediately).
- Shop-purchase checks are not in this build. Checks come from story, key
  missions, arena, and mercenary ranks (plus optional archive logs).
- The `archive_logs` option relies on archive collection flags that are still
  being verified; treat that option as experimental for now.
- Crashing during a mission will remove all items recived during that mission.
