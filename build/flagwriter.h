#pragma once
#include <cstdint>

bool ReadEventFlag(uintptr_t eventFlagMan, int32_t divisor, uint32_t flagId);
void WriteEventFlag(uintptr_t eventFlagMan, int32_t divisor,
    uint32_t flagId, bool value);