#pragma once
#include <cstdint>
#include <vector>
#include <string>

// One entry per shop slot, sourced from slot_data in the AP Connected packet.
struct AC6ShopSlot {
    int      slot;          // 0-based index
    int      chapter;       // 1-4: which chapter this slot belongs to
    uint32_t flag_id;       // event flag the EMEVD sets when item is purchased
    int64_t  ap_loc_id;     // AP location ID to send as a check
    std::string display_name; // "{player}'s {item}" — for logging
};

// Populated by ParseShopSlots() once the Connected packet arrives.
// Empty until then, or if shop_checks are disabled for this seed.
extern std::vector<AC6ShopSlot> g_shopSlots;

// Fills g_shopSlots from a Connected packet's slot_data (no-op if absent).
void ParseShopSlots(const std::string& connectedMsg);