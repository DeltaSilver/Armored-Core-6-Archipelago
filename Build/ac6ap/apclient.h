#pragma once
#include <cstdint>
#include <string>

void APClient_Connect(const char* uri, const char* slot, const char* password);
void APClient_Disconnect();
void APClient_SendCheck(int64_t locationId);
void APClient_SendGoal();
void APClient_SendDeathLink();

// Runtime (re)connect to a different room/slot without restarting the game.
// Used by the in-game F8 settings window. Safe to call while connected.
void APClient_Reconnect(const char* uri, const char* slot, const char* password);

// Connection state for the settings UI.
enum { AP_DISCONNECTED = 0, AP_CONNECTING = 1, AP_CONNECTED = 2 };
int         APClient_State();
std::string APClient_StatusText();

// How many shop batches per chapter to unlock (0-3). The DLL writes
// batch-unlock flags when each chapter's gate flag fires. 0 = shop disabled.
int  APClient_GetBatchesPerCh();

void FindAddItemFunction();
void GrantItem(int itemId, int quantity);
void QueueGrant(int itemId);