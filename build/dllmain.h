#pragma once

void LogTagged(const char* tag, const char* format, ...);

#ifndef AC6AP_LOG_TAG
#define AC6AP_LOG_TAG "???"
#endif

#define Log(...) LogTagged(AC6AP_LOG_TAG, __VA_ARGS__)

void QueueGrant(int itemId);
void SetGrantsSafe(bool safe);
void SetGarageVisited();

// Apply connection settings from the in-game settings UI: persist to ac6ap.cfg
// and (re)connect live. host/port/slot/password are UTF-8.
void AC6_ApplyConnection(const char* host, const char* port,
                         const char* slot, const char* password);