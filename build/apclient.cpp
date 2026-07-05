#include "apclient.h"
#include "dllmain.h"
#include "memory.h"
#include "partnames.h"
#include "overlay.h"

#include <windows.h>
#include <thread>
#include <atomic>
#include <mutex>
#include <queue>
#include <vector>
#include <map>
#include <string>
#include <sstream>
#include <fstream>

#include "easywsclient.hpp"
using easywsclient::WebSocket;

#define AC6AP_LOG_TAG "AP"

// ── AP / item-ID constants (must match the Python world) ─────────────────
#define AC6_BASE_ID            7700000
#define AC6_VICTORY_OFFSET     9000000
#define AC6_COAM_OFFSET        1
#define AC6_NGPLUS_OFFSET      2   // "NG+ Access" pass - logic gate, never granted
#define AC6_NGPLUSPLUS_OFFSET  3   // "NG++ Access" pass - logic gate, never granted
#define AC6_CHAPTER_OFF_LO     4   // "Chapter 2..5 Access" passes (offsets 4-7),
#define AC6_CHAPTER_OFF_HI     7   // logic gates only, never granted in-game

// ── Connection state ──────────────────────────────────────────────────────
static std::atomic<bool>  g_apRunning{ false };
static std::thread        g_apThread;
static WebSocket::pointer g_ws = nullptr;

static std::string g_uri, g_slot, g_password;
static std::mutex         g_connMutex;          // guards g_uri/g_slot/g_password
static std::atomic<bool>  g_wantConnect{ false };// should we maintain a connection
static std::atomic<bool>  g_forceReconnect{ false };
static std::atomic<bool>  g_wsOpen{ false };     // socket connected
static std::atomic<bool>  g_apConnected{ false };// AP "Connected" received

// Outgoing message queue
static std::mutex               g_sendMutex;
static std::queue<std::string>  g_sendQueue;

// ── Received-item resync state ─────────────────────────────────────────────
static int         g_processedItemCount = 0;
static std::string g_seedName = "";
static int         g_slotNumber = -1;
static bool        g_receiveStateLoaded = false;

// slot number -> display name, parsed from the Connected packet's players list.
static std::map<int, std::string> g_playerNames;
static std::string PlayerName(int slot) {
    auto it = g_playerNames.find(slot);
    return it != g_playerNames.end() ? it->second
                                     : ("Player " + std::to_string(slot));
}

// slot -> game (from Connected slot_info) and game -> (item id -> name) from the
// DataPackage. Together these name an item we send to another player's world.
static std::map<int, std::string>                              g_slotGames;
static std::map<std::string, std::map<long long, std::string>> g_itemNames;

static std::string SentItemName(int recv, long long itemId) {
    auto g = g_slotGames.find(recv);
    if (g == g_slotGames.end()) return "";
    auto m = g_itemNames.find(g->second);
    if (m == g_itemNames.end()) return "";
    auto it = m->second.find(itemId);
    return it != m->second.end() ? it->second : "";
}

// Find "key":<number> at/after `from`, write value to `out`, advance `from`.
// I honestly don't know how this works, it just does.
static bool ExtractNextInt(const std::string& s, const std::string& key,
    size_t& from, long long& out) {
    std::string pat = "\"" + key + "\":";
    size_t p = s.find(pat, from);
    if (p == std::string::npos) return false;
    p += pat.size();
    while (p < s.size() && (s[p] == ' ' || s[p] == '\t')) p++;
    bool neg = false;
    if (p < s.size() && s[p] == '-') { neg = true; p++; }
    long long val = 0; bool any = false;
    while (p < s.size() && s[p] >= '0' && s[p] <= '9') {
        val = val * 10 + (s[p] - '0'); p++; any = true;
    }
    if (!any) return false;
    out = neg ? -val : val;
    from = p;
    return true;
}

// Find "key":"value" at/after `from`, write value to `out`, advance `from`.
// (Does not unescape; player names with embedded quotes are not expected.)
static bool ExtractNextStr(const std::string& s, const std::string& key,
    size_t& from, std::string& out) {
    std::string pat = "\"" + key + "\":\"";
    size_t p = s.find(pat, from);
    if (p == std::string::npos) return false;
    p += pat.size();
    size_t e = s.find("\"", p);
    if (e == std::string::npos) return false;
    out = s.substr(p, e - p);
    from = e + 1;
    return true;
}

// Build slot -> name from the Connected packet's "players":[{slot,alias,name}].
static void ParseConnectedPlayers(const std::string& msg) {
    size_t pp = msg.find("\"players\":[");
    if (pp == std::string::npos) return;
    size_t end = msg.find(']', pp);                 // player objects contain no ']'
    std::string arr = (end == std::string::npos) ? msg.substr(pp)
                                                  : msg.substr(pp, end - pp);
    size_t scan = 0; long long slot;
    while (ExtractNextInt(arr, "slot", scan, slot)) {
        size_t s = scan; std::string nm;
        if (ExtractNextStr(arr, "alias", s, nm) ||
            (s = scan, ExtractNextStr(arr, "name", s, nm)))
            g_playerNames[(int)slot] = nm;
    }
    Log("Players parsed: %zu", g_playerNames.size());
}

// ---------------------------------------------------------------------------
//  Minimal JSON scanning, just enough to walk the DataPackage. Strings are
//  handled with escapes so item names containing quotes/colons/braces don't
//  break the walk. No allocation beyond the captured value.
// ---------------------------------------------------------------------------

// p is at the opening quote. Returns index after the closing quote; *out gets
// the (unescaped) string value if provided.
static size_t SkipString(const std::string& s, size_t p, std::string* out) {
    std::string v;
    p++;  // past opening quote
    while (p < s.size()) {
        char ch = s[p];
        if (ch == '\\') {
            if (p + 1 >= s.size()) { p++; break; }
            char e = s[p + 1];
            switch (e) {
                case 'n': v += '\n'; break;  case 't': v += '\t'; break;
                case 'u': p += 4; v += '?'; break;   // skip \uXXXX, placeholder
                default:  v += e; break;             // " \ / etc -> literal
            }
            p += 2;
        } else if (ch == '"') { p++; break; }
        else { v += ch; p++; }
    }
    if (out) *out = v;
    return p;
}

// p is at '{' or '['. Returns index of the matching close, or npos.
static size_t MatchBrace(const std::string& s, size_t p) {
    char open = s[p], close = (open == '{') ? '}' : ']';
    int depth = 0;
    while (p < s.size()) {
        char ch = s[p];
        if (ch == '"') { p = SkipString(s, p, nullptr); continue; }
        if (ch == open) depth++;
        else if (ch == close && --depth == 0) return p;
        p++;
    }
    return std::string::npos;
}

// Parse a game's "item_name_to_id":{ "Name": id, ... } into g_itemNames[game].
static void ParseItemMap(const std::string& s, size_t bstart, size_t bend,
                         const std::string& game) {
    size_t ip = s.find("\"item_name_to_id\":{", bstart);
    if (ip == std::string::npos || ip >= bend) return;
    size_t ob = ip + strlen("\"item_name_to_id\":");   // at '{'
    size_t cb = MatchBrace(s, ob);
    if (cb == std::string::npos || cb > bend) cb = bend;

    auto& m = g_itemNames[game];
    size_t p = ob + 1;
    while (p < cb) {
        while (p < cb && s[p] != '"') p++;
        if (p >= cb) break;
        std::string name;
        size_t after = SkipString(s, p, &name);
        size_t c = after;
        while (c < cb && s[c] != ':') c++;
        size_t q = c + 1;
        while (q < cb && (s[q] == ' ' || s[q] == '\t')) q++;
        bool neg = false; if (q < cb && s[q] == '-') { neg = true; q++; }
        long long id = 0; bool any = false;
        while (q < cb && s[q] >= '0' && s[q] <= '9') { id = id * 10 + (s[q] - '0'); q++; any = true; }
        if (any) m[neg ? -id : id] = name;
        p = q;
    }
}

// Walk DataPackage "data":{"games":{ "GameA":{...}, ... }} and parse each game.
static void ParseDataPackage(const std::string& msg) {
    size_t gp = msg.find("\"games\":{");
    if (gp == std::string::npos) return;
    size_t ob = gp + strlen("\"games\":");   // at '{'
    size_t cb = MatchBrace(msg, ob);
    if (cb == std::string::npos) cb = msg.size();

    int n = 0;
    size_t p = ob + 1;
    while (p < cb) {
        while (p < cb && msg[p] != '"') p++;
        if (p >= cb) break;
        std::string game;
        size_t after = SkipString(msg, p, &game);
        size_t vob = msg.find('{', after);
        if (vob == std::string::npos || vob >= cb) break;
        size_t vcb = MatchBrace(msg, vob);
        if (vcb == std::string::npos) break;
        ParseItemMap(msg, vob, vcb, game);
        n++;
        p = vcb + 1;
    }
    Log("DataPackage parsed: %d game(s)", n);
}

// Parse Connected "slot_info":{ "1":{...,"game":"X"}, ... } into g_slotGames.
static void ParseSlotInfo(const std::string& msg) {
    size_t sp = msg.find("\"slot_info\":{");
    if (sp == std::string::npos) return;
    size_t ob = sp + strlen("\"slot_info\":");   // at '{'
    size_t cb = MatchBrace(msg, ob);
    if (cb == std::string::npos) return;

    size_t p = ob + 1;
    while (p < cb) {
        while (p < cb && msg[p] != '"') p++;
        if (p >= cb) break;
        std::string slotStr;
        size_t after = SkipString(msg, p, &slotStr);
        size_t vob = msg.find('{', after);
        if (vob == std::string::npos || vob >= cb) break;
        size_t vcb = MatchBrace(msg, vob);
        if (vcb == std::string::npos) break;
        size_t g = msg.find("\"game\":\"", vob);
        if (g != std::string::npos && g < vcb) {
            std::string game;
            SkipString(msg, g + strlen("\"game\":"), &game);
            g_slotGames[atoi(slotStr.c_str())] = game;
        }
        p = vcb + 1;
    }
    Log("Slot games parsed: %zu", g_slotGames.size());
}

// From RoomInfo "games":[ "A", "B" ] build a GetDataPackage request for them.
static std::string MakeDataPackageRequest(const std::string& msg) {
    size_t gp = msg.find("\"games\":[");
    if (gp == std::string::npos) return "";
    size_t p = gp + strlen("\"games\":[");
    size_t cb = msg.find(']', p);
    std::string req = "[{\"cmd\":\"GetDataPackage\",\"games\":[";
    bool first = true;
    while (p < cb) {
        while (p < cb && msg[p] != '"') p++;
        if (p >= cb) break;
        std::string g;
        p = SkipString(msg, p, &g);
        if (g.empty()) continue;
        req += (first ? "" : ",");
        req += "\"" + g + "\"";
        first = false;
    }
    req += "]}]";
    return req;
}

// ===========================================================================
//  Receive-state persistence
// ===========================================================================

static std::string GetReceiveStatePath() {
    return GetDllDir() + "ac6ap_recv_"
        + g_seedName + "_" + std::to_string(g_slotNumber) + ".txt";
}

static void LoadReceiveState() {
    g_processedItemCount = 0;
    std::ifstream f(GetReceiveStatePath());
    if (f.is_open()) { f >> g_processedItemCount; f.close(); }
    g_receiveStateLoaded = true;
    Log("Receive state loaded: %d items already granted", g_processedItemCount);
}

static void SaveReceiveState() {
    std::ofstream f(GetReceiveStatePath(), std::ios::trunc);
    if (f.is_open()) { f << g_processedItemCount; f.close(); }
}

// ===========================================================================
//  New Game cycle persistence (NG / NG+ / NG++)
// ===========================================================================

#define AC6_NUM_CYCLES 3   // NG, NG+, NG++

static int g_ngCycle = 0;

static std::string GetCyclePath() {
    return GetDllDir() + "ac6ap_cycle_"
        + g_seedName + "_" + std::to_string(g_slotNumber) + ".txt";
}

static void LoadCycle() {
    g_ngCycle = 0;
    std::ifstream f(GetCyclePath());
    if (f.is_open()) { f >> g_ngCycle; f.close(); }
    if (g_ngCycle < 0) g_ngCycle = 0;
    if (g_ngCycle > AC6_NUM_CYCLES - 1) g_ngCycle = AC6_NUM_CYCLES - 1;
    Log("NG cycle loaded: %d (0=NG 1=NG+ 2=NG++)", g_ngCycle);
}

static void SaveCycle() {
    std::ofstream f(GetCyclePath(), std::ios::trunc);
    if (f.is_open()) { f << g_ngCycle; f.close(); }
}

int APClient_GetCycle() {
    return g_ngCycle;
}

// Run mode from slot_data: 0 single, 1 ng_plus_run, 2 ng_plus_run_cycled,
// 3 full_run, 4 full_run_cycled.
static int g_runMode = 0;

// How many NG cycles this mode spans (single=1, ng_plus_*=2, full_*=3).
static int CyclesForMode(int mode) {
    switch (mode) {
        case 1: case 2: return 2;   // ng_plus runs
        case 3: case 4: return 3;   // full runs
        default:        return 1;   // single (0) / unknown
    }
}

// The "_cycled" modes (2, 4) duplicate checks per cycle, so only they advance
// the cycle counter. The others keep cycle 0 and dedupe re-fires server-side.
static bool IsCycledMode(int mode) { return mode == 2 || mode == 4; }

int APClient_GetRunMode() {
    return g_runMode;
}

int APClient_GetRequiredEndings() {
    return CyclesForMode(g_runMode);
}

void APClient_AdvanceCycle() {
    if (!IsCycledMode(g_runMode)) return;
    int maxCycle = CyclesForMode(g_runMode) - 1;   // 1 for ng_plus, 2 for full
    if (g_ngCycle >= maxCycle) {
        Log("AdvanceCycle: at max cycle (%d) for mode %d, not advancing",
            g_ngCycle, g_runMode);
        return;
    }
    g_ngCycle++;
    SaveCycle();
    Log("NG cycle advanced to %d (0=NG 1=NG+ 2=NG++)", g_ngCycle);
}

// ===========================================================================
//  Outgoing messages
// ===========================================================================

static void QueueMessage(const std::string& msg) {
    std::lock_guard<std::mutex> lk(g_sendMutex);
    g_sendQueue.push(msg);
}

static std::string MakeConnectPacket() {
    std::string slot, pw;
    { std::lock_guard<std::mutex> lk(g_connMutex); slot = g_slot; pw = g_password; }
    std::ostringstream o;
    o << "[{\"cmd\":\"Connect\","
        << "\"game\":\"Armored Core VI\","
        << "\"name\":\"" << slot << "\","
        << "\"password\":\"" << pw << "\","
        << "\"version\":{\"major\":0,\"minor\":6,\"build\":0,\"class\":\"Version\"},"
        << "\"items_handling\":7,"
        << "\"tags\":[],"
        << "\"uuid\":\"ac6ap\","
        << "\"slot_data\":true}]";
    return o.str();
}

void APClient_SendCheck(int64_t locationId) {
    Log("Sending check: %lld", locationId);
    std::ostringstream o;
    o << "[{\"cmd\":\"LocationChecks\",\"locations\":[" << locationId << "]}]";
    QueueMessage(o.str());
}

void APClient_SendGoal() {
    Log("Sending goal completion");
    QueueMessage("[{\"cmd\":\"StatusUpdate\",\"status\":30}]");
}

// ===========================================================================
//  Granting received items
// ===========================================================================

static void OnItemReceived(long long apItemId, int finder) {
    long long offset = apItemId - AC6_BASE_ID;
    if (offset == AC6_VICTORY_OFFSET) { Log("Received Victory (no grant)"); return; }
    if (offset == AC6_COAM_OFFSET) { Log("Received COAM filler (no grant)"); return; }
    if (offset == AC6_NGPLUS_OFFSET || offset == AC6_NGPLUSPLUS_OFFSET) {
        Log("Received NG cycle access pass (no grant)"); return;
    }
    if (offset >= AC6_CHAPTER_OFF_LO && offset <= AC6_CHAPTER_OFF_HI) {
        Log("Received chapter access pass (no grant)"); return;
    }
    if (offset < 0) { Log("Item %lld out of range (skipped)", apItemId); return; }

    // "from <player>" only when someone else found it (not our own location).
    char from[96] = "";
    if (finder >= 0 && finder != g_slotNumber)
        snprintf(from, sizeof(from), "  (from %s)", PlayerName(finder).c_str());

    const char* name = GetPartName((int)offset);
    if (name) {
        Log("Queuing: %s%s", name, from);
        Overlay_Message(OVL_RECEIVED, "Received  %s%s", name, from);
    } else {
        Log("Queuing unknown part 0x%X", (unsigned int)offset);
        Overlay_Message(OVL_RECEIVED, "Received  part 0x%X%s", (unsigned int)offset, from);
    }

    QueueGrant((int)offset);
}

// Handle a ReceivedItems packet. The "index" field is the absolute position
// of the first item in the player's overall received list; we only grant
// items at positions >= what we've already processed, so reconnects (which
// resend from index 0) never double-grant.
// Future plan: replace this with more in-depth item logging
static void HandleReceivedItems(const std::string& msg) {
    if (!g_receiveStateLoaded) LoadReceiveState();

    size_t tmp = 0;
    long long startIndex = 0;
    ExtractNextInt(msg, "index", tmp, startIndex);

    size_t itemsPos = msg.find("\"items\":[");
    if (itemsPos == std::string::npos) { Log("ReceivedItems: no items array"); return; }

    size_t scan = itemsPos;
    long long pos = startIndex;
    long long itemId;
    while (ExtractNextInt(msg, "item", scan, itemId)) {
        // "player" (the finder) follows "item" in the same NetworkItem object.
        long long finder = -1; size_t fs = scan;
        ExtractNextInt(msg, "player", fs, finder);
        if (pos >= g_processedItemCount) {
            OnItemReceived(itemId, (int)finder);
            g_processedItemCount = (int)(pos + 1);
            SaveReceiveState();
        }
        pos++;
    }
    Log("ReceivedItems processed up to %d", g_processedItemCount);
}

// A PrintJSON "ItemSend" line: announce items WE found that go to someone else.
// Shape: ...,"type":"ItemSend","receiving":<recv>,"item":{"item":..,"location":..,
//        "player":<finder>,..}. We are the finder for things we send out.
static void HandleItemSend(const std::string& msg) {
    size_t t = 0; long long recv = -1;
    ExtractNextInt(msg, "receiving", t, recv);

    size_t ip = msg.find("\"item\":{");
    if (ip == std::string::npos) return;
    size_t s = ip + strlen("\"item\":{");
    long long itemId = -1, locId = -1, finder = -1;
    ExtractNextInt(msg, "item", s, itemId);
    ExtractNextInt(msg, "location", s, locId);
    ExtractNextInt(msg, "player", s, finder);

    if (finder == g_slotNumber && recv >= 0 && recv != g_slotNumber) {
        std::string iname = SentItemName((int)recv, itemId);
        if (!iname.empty())
            Overlay_Message(OVL_SENT, "Sent %s to %s", iname.c_str(), PlayerName((int)recv).c_str());
        else
            Overlay_Message(OVL_SENT, "Sent to %s", PlayerName((int)recv).c_str());
    }
}

// ===========================================================================
//  Incoming message dispatch
// ===========================================================================

static void HandleMessage(const std::string& msg) {
    Log("AP received: %.400s", msg.c_str());

    // RoomInfo -> reply with Connect, capture seed name
    if (msg.find("\"cmd\":\"RoomInfo\"") != std::string::npos) {
        size_t sp = msg.find("\"seed_name\":\"");
        if (sp != std::string::npos) {
            sp += strlen("\"seed_name\":\"");
            size_t ep = msg.find("\"", sp);
            if (ep != std::string::npos) g_seedName = msg.substr(sp, ep - sp);
        }
        QueueMessage(MakeConnectPacket());
        std::string dp = MakeDataPackageRequest(msg);   // names for items we send
        if (!dp.empty()) QueueMessage(dp);
    }

    // DataPackage -> build game -> item id -> name maps
    if (msg.find("\"cmd\":\"DataPackage\"") != std::string::npos) {
        ParseDataPackage(msg);
    }

    // Connected -> capture slot, load persisted grant count
    if (msg.find("\"cmd\":\"Connected\"") != std::string::npos) {
        size_t t = 0; long long slot = -1;
        ExtractNextInt(msg, "slot", t, slot);   // top-level slot precedes players[]
        g_slotNumber = (int)slot;
        g_apConnected = true;

        // Run mode from slot_data (default single=0 if absent).
        size_t rm = 0; long long mode = 0;
        if (ExtractNextInt(msg, "run_mode", rm, mode)) g_runMode = (int)mode;
        Log("Run mode: %d (0=single 1=ng+ 2=ng+cycled 3=full 4=full-cycled)", g_runMode);

        ParseConnectedPlayers(msg);   // slot -> name, for "from/to" overlay text
        ParseSlotInfo(msg);           // slot -> game, to name items we send
        LoadReceiveState();
        LoadCycle();
    }

    // ReceivedItems -> grant
    if (msg.find("\"cmd\":\"ReceivedItems\"") != std::string::npos) {
        HandleReceivedItems(msg);
    }

    // Item we found that goes to another player -> "Sent to <player>"
    if (msg.find("\"type\":\"ItemSend\"") != std::string::npos) {
        HandleItemSend(msg);
    }
}

// ===========================================================================
//  Websocket thread
// ===========================================================================

// The thread is persistent for the life of the DLL; whether it actually holds a
// connection is gated on g_wantConnect, so Connect/Disconnect/Reconnect never
// start or stop the thread (no restart races).
static void APThread() {
    Log("AP thread started");

    while (g_apRunning) {
        if (!g_wantConnect) {            // idle until something asks to connect
            g_wsOpen = false; g_apConnected = false;
            Sleep(200);
            continue;
        }

        std::string uri;
        { std::lock_guard<std::mutex> lk(g_connMutex); uri = g_uri; }
        if (uri.empty()) { Sleep(200); continue; }

        g_wsOpen = false; g_apConnected = false;
        g_forceReconnect = false;

        g_ws = WebSocket::from_url(uri);   // blocks during the TCP connect
        if (!g_ws) {
            Log("AP: connect failed, retrying...");
            for (int i = 0; i < 50 && g_apRunning && g_wantConnect && !g_forceReconnect; i++)
                Sleep(100);
            continue;
        }
        g_wsOpen = true;
        Log("AP: connected to %s", uri.c_str());

        bool forced = false;
        while (g_apRunning && g_wantConnect && g_ws->getReadyState() != WebSocket::CLOSED) {
            if (g_forceReconnect.exchange(false)) { forced = true; break; }
            {
                std::lock_guard<std::mutex> lk(g_sendMutex);
                while (!g_sendQueue.empty()) {
                    g_ws->send(g_sendQueue.front());
                    g_sendQueue.pop();
                }
            }
            g_ws->poll();
            g_ws->dispatch([](const std::string& message) { HandleMessage(message); });
            Sleep(50);
        }

        g_wsOpen = false; g_apConnected = false;
        if (g_ws) { delete g_ws; g_ws = nullptr; }
        if (!g_wantConnect)  Log("AP: disconnected by request");
        else if (forced)     Log("AP: reconnecting (settings changed)");
        else {
            Log("AP: connection lost");   // brief backoff before auto-reconnect
            for (int i = 0; i < 30 && g_apRunning && g_wantConnect && !g_forceReconnect; i++)
                Sleep(100);
        }
    }

    if (g_ws) { delete g_ws; g_ws = nullptr; }
    g_wsOpen = false; g_apConnected = false;
    Log("AP thread stopped");
}

// Reset connection-scoped state so a different room/slot starts clean.
static void ResetConnState() {
    g_slotNumber = -1;
    g_seedName = "";
    g_processedItemCount = 0;
    g_receiveStateLoaded = false;
    g_playerNames.clear();
    g_slotGames.clear();
    g_itemNames.clear();
}

// Ensure the (single, persistent) AP thread is running.
static void EnsureThread() {
    if (g_apRunning.exchange(true)) return;
    g_apThread = std::thread(APThread);
    g_apThread.detach();
}

void APClient_Connect(const char* uri, const char* slot, const char* password) {
    APClient_Reconnect(uri, slot, password);
}

void APClient_Reconnect(const char* uri, const char* slot, const char* password) {
    { std::lock_guard<std::mutex> lk(g_connMutex); g_uri = uri; g_slot = slot; g_password = password; }
    ResetConnState();
    g_apConnected = false;
    g_wantConnect = true;
    g_forceReconnect = true;            // recycle if already connected
    Log("Connecting to AP server: %s as slot: %s", uri, slot);
    EnsureThread();
}

std::string APClient_StatusText() {
    if (g_apConnected) {
        std::string slot;
        { std::lock_guard<std::mutex> lk(g_connMutex); slot = g_slot; }
        return "Connected as " + slot;
    }
    if (!g_wantConnect) return "Disconnected";
    if (g_wsOpen)       return "Authenticating...";
    return "Connecting...";
}

int APClient_State() {
    if (g_apConnected) return AP_CONNECTED;
    if (g_wantConnect) return AP_CONNECTING;
    return AP_DISCONNECTED;
}

void APClient_Disconnect() {
    g_wantConnect = false;
    g_apConnected = false;
    g_forceReconnect = true;            // break the inner loop and drop the socket
    Log("Disconnect requested");
}