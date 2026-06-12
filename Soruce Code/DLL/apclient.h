#pragma once
#include <cstdint>
#include <string>

void APClient_Connect(const char* uri, const char* slot, const char* password);
void APClient_Disconnect();
void APClient_SendCheck(int64_t locationId);
void APClient_SendGoal();
void FindAddItemFunction();
void GrantItem(int itemId, int quantity);
void QueueGrant(int itemId);