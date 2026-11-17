#include "flagwriter.h"

#define AC6AP_LOG_TAG "FLAG_WRITE"

bool ReadEventFlag(uintptr_t eventFlagMan, int32_t divisor, uint32_t flagId) {
    uint32_t category = flagId / divisor;
    uint32_t leastSig = flagId - (category * divisor);

    uintptr_t currentElement = *(uintptr_t*)(eventFlagMan + 0x38);
    if (!currentElement) return false;

    uintptr_t currentSubElement = *(uintptr_t*)(currentElement + 0x8);
    if (!currentSubElement) return false;

    while (*(uint8_t*)(currentSubElement + 0x19) == '\0') {
        if (*(int32_t*)(currentSubElement + 0x20) < (int32_t)category)
            currentSubElement = *(uintptr_t*)(currentSubElement + 0x10);
        else {
            currentElement = currentSubElement;
            currentSubElement = *(uintptr_t*)(currentSubElement + 0x0);
        }
        if (!currentSubElement) return false;
    }

    if ((int32_t)category < *(int32_t*)(currentElement + 0x20))
        return false;
    if (currentElement == currentSubElement)
        return false;

    int32_t mysteryValue = *(int32_t*)(currentElement + 0x28) - 1;
    long long calculatedPointer = 0;

    if (mysteryValue == 0) {
        calculatedPointer =
            (*(int32_t*)(eventFlagMan + 0x20) * *(int32_t*)(currentElement + 0x30))
            + *(long long*)(eventFlagMan + 0x28);
    }
    else if (mysteryValue == 2) {
        calculatedPointer = *(long long*)(currentElement + 0x30);
    }
    else {
        return false;
    }

    if (!calculatedPointer) return false;

    uint32_t thing = 7 - (leastSig & 7);
    int32_t mask = 1 << thing;
    uint32_t shifted = leastSig >> 3;

    int32_t value = *(int32_t*)(calculatedPointer + shifted);
    return (value & mask) != 0;
}

void WriteEventFlag(uintptr_t eventFlagMan, int32_t divisor,
    uint32_t flagId, bool value) {
    uint32_t category = flagId / divisor;
    uint32_t leastSig = flagId - (category * divisor);

    uintptr_t currentElement = *(uintptr_t*)(eventFlagMan + 0x38);
    if (!currentElement) return;

    uintptr_t currentSubElement = *(uintptr_t*)(currentElement + 0x8);
    if (!currentSubElement) return;

    while (*(uint8_t*)(currentSubElement + 0x19) == '\0') {
        if (*(int32_t*)(currentSubElement + 0x20) < (int32_t)category)
            currentSubElement = *(uintptr_t*)(currentSubElement + 0x10);
        else {
            currentElement = currentSubElement;
            currentSubElement = *(uintptr_t*)(currentSubElement + 0x0);
        }
        if (!currentSubElement) return;
    }

    if ((int32_t)category < *(int32_t*)(currentElement + 0x20))
        return;
    if (currentElement == currentSubElement)
        return;

    int32_t mysteryValue = *(int32_t*)(currentElement + 0x28) - 1;
    long long calculatedPointer = 0;

    if (mysteryValue == 0) {
        calculatedPointer =
            (*(int32_t*)(eventFlagMan + 0x20) * *(int32_t*)(currentElement + 0x30))
            + *(long long*)(eventFlagMan + 0x28);
    }
    else if (mysteryValue == 2) {
        calculatedPointer = *(long long*)(currentElement + 0x30);
    }
    else {
        return;
    }

    if (!calculatedPointer) return;

    uint32_t thing = 7 - (leastSig & 7);
    int32_t mask = 1 << thing;
    uint32_t shifted = leastSig >> 3;

    int32_t current = *(int32_t*)(calculatedPointer + shifted);
    if (value)
        *(int32_t*)(calculatedPointer + shifted) = current | mask;
    else
        *(int32_t*)(calculatedPointer + shifted) = current & ~mask;
}