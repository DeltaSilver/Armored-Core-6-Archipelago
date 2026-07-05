#pragma once

// Lightweight on-screen message feed for Archipelago events. Implemented as a
// separate layered, click-through, top-most window drawn with GDI - it never
// hooks the game's renderer, so it cannot crash the game or clash with
// ModEngine2. Shows over borderless/windowed mode (the FromSoft default), not
// exclusive fullscreen.

enum AC6OverlayKind {
    OVL_RECEIVED = 0,   // item granted to us           (green)
    OVL_SENT     = 1,   // a check we completed / sent   (cyan)
    OVL_INFO     = 2,   // connection / cycle / goal     (white)
};

void Overlay_Start();                              // create window + render thread
void Overlay_Stop();
void Overlay_Message(AC6OverlayKind kind, const char* fmt, ...);
