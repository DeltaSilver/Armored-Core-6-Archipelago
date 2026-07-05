#define AC6AP_LOG_TAG "SHOP"
#include "dllmain.h"
#include "shopslots.h"

#include <string>
#include <vector>

std::vector<AC6ShopSlot> g_shopSlots;

// Minimal JSON field extractors — no external library needed.
// Same approach as apclient.cpp's ExtractNextInt, duplicated here to keep
// this file standalone (it has no dependency on apclient.cpp internals).

static bool ExtractInt64(const std::string& s, const std::string& key,
    size_t& pos, int64_t& out) {
    std::string pat = "\"" + key + "\":";
    size_t p = s.find(pat, pos);
    if (p == std::string::npos) return false;
    p += pat.size();
    while (p < s.size() && (s[p] == ' ' || s[p] == '\t')) p++;
    bool neg = false;
    if (p < s.size() && s[p] == '-') { neg = true; p++; }
    int64_t val = 0; bool any = false;
    while (p < s.size() && s[p] >= '0' && s[p] <= '9') {
        val = val * 10 + (s[p++] - '0'); any = true;
    }
    if (!any) return false;
    out = neg ? -val : val;
    pos = p;
    return true;
}

static bool ExtractString(const std::string& s, const std::string& key,
    size_t& pos, std::string& out) {
    std::string pat = "\"" + key + "\":\"";
    size_t p = s.find(pat, pos);
    if (p == std::string::npos) return false;
    p += pat.size();
    size_t end = s.find('"', p);
    if (end == std::string::npos) return false;
    out = s.substr(p, end - p);
    pos = end + 1;
    return true;
}

void ParseShopSlots(const std::string& connectedMsg) {
    g_shopSlots.clear();

    // shop_slots lives inside slot_data: {"slot_data":{...,"shop_slots":[...]}}
    size_t arr_pos = connectedMsg.find("\"shop_slots\"");
    if (arr_pos == std::string::npos) {
        Log("No shop_slots in slot_data — shop checks disabled this seed");
        return;
    }

    size_t arr_start = connectedMsg.find('[', arr_pos);
    if (arr_start == std::string::npos) {
        Log("shop_slots found but malformed (no array)");
        return;
    }

    // Find the matching closing bracket for the array so we don't walk
    // past it into unrelated slot_data fields that follow.
    int depth = 0;
    size_t arr_end = std::string::npos;
    for (size_t i = arr_start; i < connectedMsg.size(); i++) {
        if (connectedMsg[i] == '[') depth++;
        else if (connectedMsg[i] == ']') {
            depth--;
            if (depth == 0) { arr_end = i; break; }
        }
    }
    if (arr_end == std::string::npos) {
        Log("shop_slots array has no closing bracket");
        return;
    }

    std::string arr = connectedMsg.substr(arr_start, arr_end - arr_start + 1);

    size_t scan = 0;
    int loaded = 0;

    while (true) {
        size_t obj_start = arr.find('{', scan);
        if (obj_start == std::string::npos) break;
        size_t obj_end = arr.find('}', obj_start);
        if (obj_end == std::string::npos) break;

        std::string obj = arr.substr(obj_start, obj_end - obj_start + 1);
        size_t p = 0;

        int64_t slot_n = -1, chapter = -1, flag_id = -1, ap_loc_id = -1;
        std::string display_name;

        ExtractInt64(obj, "slot", p, slot_n);
        p = 0;
        ExtractInt64(obj, "chapter", p, chapter);
        p = 0;
        ExtractInt64(obj, "flag_id", p, flag_id);
        p = 0;
        ExtractInt64(obj, "ap_loc_id", p, ap_loc_id);
        p = 0;
        ExtractString(obj, "display_name", p, display_name);

        if (slot_n >= 0 && chapter >= 1 && chapter <= 4 && flag_id >= 0 && ap_loc_id >= 0) {
            AC6ShopSlot ss;
            ss.slot = (int)slot_n;
            ss.chapter = (int)chapter;
            ss.flag_id = (uint32_t)flag_id;
            ss.ap_loc_id = ap_loc_id;
            ss.display_name = display_name.empty()
                ? "AP Slot " + std::to_string(slot_n)
                : display_name;
            g_shopSlots.push_back(ss);
            loaded++;
        }

        scan = obj_end + 1;
    }

    Log("Loaded %d shop slots from slot_data", loaded);
}