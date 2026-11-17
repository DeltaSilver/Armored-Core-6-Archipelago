#pragma once
#include <cstdint>

void LogTagged(const char* tag, const char* format, ...);

#ifndef AC6AP_LOG_TAG
#define AC6AP_LOG_TAG "???"
#endif

#define Log(...) LogTagged(AC6AP_LOG_TAG, __VA_ARGS__)

void QueueGrant(int itemId);
void QueueGrantCOAM(int32_t amount);
void SetGrantsSafe(bool safe);
void SetGarageVisited();

void AC6_ApplyConnection(const char* host, const char* port,
                         const char* slot, const char* password);
