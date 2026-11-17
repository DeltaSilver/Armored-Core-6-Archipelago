#pragma once

// On-screen AP message feed: a separate GDI window, not a renderer hook.
// Borderless/windowed mode only.

enum AC6OverlayKind {
    OVL_RECEIVED = 0,   // item granted to us           (green)
    OVL_SENT = 1,   // a check we completed / sent   (cyan)
    OVL_INFO = 2,   // connection / goal             (white)
};

void Overlay_Start();                              // create window + render thread
void Overlay_Stop();
void Overlay_Message(AC6OverlayKind kind, const char* fmt, ...);

// Scales the overlay window, text, and spacing. 1.0 = default size. Call BEFORE
// Overlay_Start (it's read once at window creation); clamped to a sane range.
void Overlay_SetScale(float scale);