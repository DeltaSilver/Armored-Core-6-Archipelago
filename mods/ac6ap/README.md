# ac6ap - ModEngine2 mod folder

Copy the contents of this folder into your ModEngine2 `mod/` directory, then
register `ac6ap.dll` in `config_armoredcore6.toml` (`external_dlls = ["mod/ac6ap.dll"]`).

This folder ships with `ac6ap.cfg`. Two more files are not committed and must be
added yourself:

| File | Where it comes from |
|------|---------------------|
| `ac6ap.dll` | Build it from [`../../build/`](../../build) - see [`build/BUILDING.md`](../../build/BUILDING.md) - then drop the compiled DLL here. |
| `regulation.bin` | The modded regulation from the mod release (it adjusts the part economy so parts come from Archipelago). Not needed for `discover=1` data-gathering runs. |

`ac6ap.cfg` is your connection config - edit it with your room details. If it's
missing, the DLL writes a default one here on first launch.

Runtime files the DLL writes here (all gitignored): `ac6ap_log.txt`,
`ac6ap_checks.txt` (every check sent, with its mission name - check this if a
mission seems to fire the wrong check), `ac6ap_discovery.txt`, `ac6ap_recv_*.txt`,
`ac6ap_cycle_*.txt`.
