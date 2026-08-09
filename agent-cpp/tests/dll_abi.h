#pragma once
// Shared ABI helpers for tests that P/Invoke aoi_agent.dll.
//
// The DLL exports its functions by their readable names (default dllexport).
#include <windows.h>

inline void* AoiProc(HMODULE dll, const char* name) {
  return reinterpret_cast<void*>(GetProcAddress(dll, name));
}
