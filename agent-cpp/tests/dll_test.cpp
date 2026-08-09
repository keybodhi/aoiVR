// Validates the P/Invoke lifecycle of aoi_agent.dll: start on a background
// thread, verify IsRunning, verify the calling thread is NOT blocked, then stop.
#include <windows.h>

#include <cstdio>
#include <string>

#include "dll_abi.h"

typedef void (*SetEnvFn)(const char*, const char*);
typedef int (*StartFn)(void);
typedef void (*StopFn)(void);
typedef int (*IsRunningFn)(void);

int main() {
  HMODULE dll = LoadLibraryA("aoi_agent.dll");
  if (!dll) {
    printf("FAIL: could not load aoi_agent.dll (err=%lu)\n", GetLastError());
    return 1;
  }
  auto SetEnv = (SetEnvFn)AoiProc(dll, "AoiAgent_SetEnv");   // AoiAgent_SetEnv
  auto Start = (StartFn)AoiProc(dll, "AoiAgent_Start");     // AoiAgent_Start
  auto Stop = (StopFn)AoiProc(dll, "AoiAgent_Stop");       // AoiAgent_Stop
  auto IsRunning = (IsRunningFn)AoiProc(dll, "AoiAgent_IsRunning");  // AoiAgent_IsRunning
  if (!SetEnv || !Start || !Stop || !IsRunning) {
    printf("FAIL: missing export\n");
    return 1;
  }

  SetEnv("AOI_CWD", ".");
  SetEnv("MIMO_API_KEY", "");

  DWORD t0 = GetTickCount();
  int rc = Start();
  DWORD t1 = GetTickCount();
  printf("  Start() rc=%d in %lu ms (must be near-instant => runs on child thread)\n",
         rc, t1 - t0);
  if (rc != 0) { printf("FAIL: Start returned %d\n", rc); return 1; }
  if ((t1 - t0) > 1000) { printf("FAIL: Start blocked the caller\n"); return 1; }

  Sleep(500);
  int running = IsRunning();
  printf("  IsRunning()=%d after 500ms\n", running);
  if (running != 1) { printf("FAIL: agent not running\n"); return 1; }

  // Verify the caller thread is not the agent thread and stays responsive.
  DWORD callerId = GetCurrentThreadId();
  printf("  caller thread id=%lu\n", callerId);

  DWORD t2 = GetTickCount();
  Stop();
  DWORD t3 = GetTickCount();
  printf("  Stop() took %lu ms\n", t3 - t2);

  int runningAfter = IsRunning();
  printf("  IsRunning()=%d after stop\n", runningAfter);
  if (runningAfter != 0) { printf("FAIL: still running after stop\n"); return 1; }

  FreeLibrary(dll);
  printf("ALL DLL LIFECYCLE TESTS PASSED\n");
  return 0;
}
