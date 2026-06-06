# Armored Core 6 Archipelago

A multiworld randomiser for Armored Core VI. Story, arena, and mercenary-rank
progress send Archipelago checks; the AC parts you receive are shuffled across
the multiworld.

# Intructions

Installing the mod (.dll)
Set up ModEngine2 for Armored Core VI.
Place ac6ap.dll, ac6ap.cfg and mod folder in your mod folder.
Add ac6ap.dll to the external_dlls list in your ME2 config:external_dlls = ["ac6ap.dll"]
Edit ac6ap.cfg with your room's host, port, slot name, and password.
Launch the game through ME2's .bat file.


slot must match the name in your YAML exactly. Leave password blank if
the room has none.

# Notes / known limits

Item-acquisition notifications don't pop on screen; check your AP client to
see what you received. Parts appear in the assembly menu (back out and
re-enter the menu if you don't see one immediately).

Shop-purchase checks are not in this build. Checks come from story, key
missions, arena, and mercenary ranks (plus optional archive logs).

The archive_logs option relies on archive collection flags that are still
being verified; treat that option as experimental for now.
