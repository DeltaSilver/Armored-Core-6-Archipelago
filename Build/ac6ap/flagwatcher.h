#pragma once
#include <cstdint>

void FlagWatcher_Start(uintptr_t eventFlagMan);
void FlagWatcher_Stop();
void FlagWatcher_TriggerDeathLink();