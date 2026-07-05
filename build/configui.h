#pragma once

// In-game Archipelago settings window: lets the player set host / port / slot /
// password and (re)connect at runtime, with no cfg edit or game restart. It is a
// normal top-most Win32 window (it takes keyboard focus), toggled by a hotkey.
// Shows in borderless/windowed mode.

void ConfigUI_Start(const char* host, const char* port,
                    const char* slot, const char* password);
void ConfigUI_Stop();
void ConfigUI_Toggle();   // show/hide; safe to call from another thread
