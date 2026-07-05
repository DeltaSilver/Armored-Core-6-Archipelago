#pragma once
#include <cstdint>

uintptr_t AOBScan(const char* pattern);
uintptr_t ResolveRelativePointer(uintptr_t addr, int offsetPos, int instrSize);
#include <string>
std::string GetDllDir();