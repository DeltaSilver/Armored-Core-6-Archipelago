#pragma once
#include <cstdint>
#include <string>

void APClient_Connect(const char* uri, const char* slot, const char* password);
void APClient_Disconnect();
void APClient_SendCheck(int64_t locationId);
void APClient_SendGoal();

// New Game cycle (0 = NG, 1 = NG+, 2 = NG++). Persisted per seed+slot so each
// playthrough cycle sends a distinct set of checks for the same mission flags.
int  APClient_GetCycle();
void APClient_AdvanceCycle();

// Run mode from slot_data: 0 single, 1 ng_plus_run, 2 ng_plus_run_cycled,
// 3 full_run, 4 full_run_cycled.
int  APClient_GetRunMode();

// Endings required to complete the goal for the current run mode
// (single=1 / ng_plus_*=2 / full_*=3). Ending flags persist, so the goal fires
// once this many of {6000,6001,6002} are set.
int  APClient_GetRequiredEndings();
void FindAddItemFunction();
void GrantItem(int itemId, int quantity);
void QueueGrant(int itemId);