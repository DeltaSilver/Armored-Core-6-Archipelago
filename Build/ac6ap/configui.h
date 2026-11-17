#pragma once

// F8 settings window: set host/port/slot/password and reconnect at runtime.

void ConfigUI_Start(const char* host, const char* port,
    const char* slot, const char* password);
void ConfigUI_Stop();
void ConfigUI_Toggle();   // show/hide; safe to call from another thread

// Scales the settings window and its controls. 1.0 = default size. Call BEFORE
// ConfigUI_Start (read once at window creation); clamped to a sane range.
void ConfigUI_SetScale(float scale);