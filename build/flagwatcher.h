#pragma once
#include <cstdint>

void FlagWatcher_Start(uintptr_t eventFlagMan);
void FlagWatcher_Stop();

// Discovery mode: instead of sending AP checks, scan the given flag ranges
// ("a-b,c-d,...") every poll and log every flag that flips to ac6ap_discovery.txt
// with a timestamp. Used to map missions -> trigger flags in one playthrough.
void FlagWatcher_StartDiscovery(uintptr_t eventFlagMan, const char* ranges);