#include "memory.h"
#include "dllmain.h"

#include <windows.h>
#include <psapi.h>
#include <vector>
#include <string>

// Parse a pattern string into bytes + mask.
// Supports single "?" and double "??" wildcard tokens.
static void ParsePattern(const char* pattern,
    std::vector<uint8_t>& bytes,
    std::vector<bool>& mask) {
    const char* p = pattern;
    while (*p) {
        if (*p == ' ') { p++; continue; }

        if (*p == '?') {
            // consume one or two '?' as a single wildcard byte
            bytes.push_back(0);
            mask.push_back(false);
            p++;
            if (*p == '?') p++;
        }
        else {
            // parse two hex chars
            auto hexval = [](char c) -> int {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                return 0;
                };
            int hi = hexval(*p++);
            int lo = hexval(*p++);
            bytes.push_back((uint8_t)((hi << 4) | lo));
            mask.push_back(true);
        }
    }
}

uintptr_t AOBScan(const char* pattern) {
    std::vector<uint8_t> bytes;
    std::vector<bool> mask;
    ParsePattern(pattern, bytes, mask);
    if (bytes.empty()) return 0;

    HMODULE hModule = GetModuleHandleA("armoredcore6.exe");
    if (!hModule) {
        Log("AOBScan: could not get armoredcore6.exe module");
        return 0;
    }

    MODULEINFO modInfo = {};
    if (!GetModuleInformation(GetCurrentProcess(), hModule,
        &modInfo, sizeof(modInfo))) {
        Log("AOBScan: GetModuleInformation failed");
        return 0;
    }

    uintptr_t base = (uintptr_t)modInfo.lpBaseOfDll;
    uintptr_t end = base + modInfo.SizeOfImage;
    size_t patLen = bytes.size();

    Log("AOBScan: scanning from 0x%llX to 0x%llX for pattern: %s",
        base, end, pattern);

    for (uintptr_t addr = base; addr < end - patLen; addr++) {
        const uint8_t* mem = (const uint8_t*)addr;
        bool found = true;
        for (size_t i = 0; i < patLen; i++) {
            if (mask[i] && mem[i] != bytes[i]) {
                found = false;
                break;
            }
        }
        if (found) {
            Log("AOBScan: found at 0x%llX", addr);
            return addr;
        }
    }

    Log("AOBScan: pattern not found");
    return 0;
}

uintptr_t ResolveRelativePointer(uintptr_t addr, int offsetPos, int instrSize) {
    int32_t rel = *(int32_t*)(addr + offsetPos);
    uintptr_t result = addr + instrSize + rel;
    Log("ResolveRelativePointer: 0x%llX + %d + %d = 0x%llX",
        addr, instrSize, rel, result);
    return result;
}

std::string GetDllDir() {
    char path[MAX_PATH] = { 0 };
    HMODULE hm = nullptr;
    GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCSTR)&GetDllDir, &hm);
    GetModuleFileNameA(hm, path, MAX_PATH);   // ...\<mod folder>\ac6ap.dll
    std::string p(path);
    size_t slash = p.find_last_of("\\/");
    return (slash == std::string::npos) ? "" : p.substr(0, slash + 1);
}