// Verifies the in-process transport of aoi_agent.dll (no named pipe):
//   - outbound messages (greeting) reach a registered callback;
//   - AoiAgent_SendJson feeds inbound messages (StateChange / ScreenshotResponse)
//     into the agent and the fast-path resolve still works.
#include <windows.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include "dll_abi.h"

typedef void (*SetEnvFn)(const char*, const char*);
typedef int (*StartFn)(void);
typedef void (*StopFn)(void);
typedef int (*IsRunningFn)(void);
typedef void (*SetCallbackFn)(void*);
typedef int (*SendJsonFn)(const char*);

namespace {

std::atomic<int> g_outbound{0};
std::string g_lastOutbound;
HANDLE g_callbackThread;

void __stdcall onMessage(const char* json) {
  g_outbound++;
  g_lastOutbound = json ? json : "";
}

} // namespace

int main() {
  HMODULE dll = LoadLibraryA("aoi_agent.dll");
  if (!dll) { printf("FAIL: could not load aoi_agent.dll\n"); return 1; }
  auto SetEnv = (SetEnvFn)AoiProc(dll, "AoiAgent_SetEnv");       // AoiAgent_SetEnv
  auto Start = (StartFn)AoiProc(dll, "AoiAgent_Start");         // AoiAgent_Start
  auto Stop = (StopFn)AoiProc(dll, "AoiAgent_Stop");           // AoiAgent_Stop
  auto IsRunning = (IsRunningFn)AoiProc(dll, "AoiAgent_IsRunning"); // AoiAgent_IsRunning
  auto SetCallback = (SetCallbackFn)AoiProc(dll, "AoiAgent_SetMessageCallback");  // AoiAgent_SetMessageCallback
  auto SendJson = (SendJsonFn)AoiProc(dll, "AoiAgent_SendJson");        // AoiAgent_SendJson
  if (!SetEnv || !Start || !Stop || !IsRunning || !SetCallback || !SendJson) {
    printf("FAIL: missing export\n"); return 1;
  }

  SetEnv("AOI_CWD", ".");
  SetEnv("OPENCODE_API_KEY", "sk-test");
  SetEnv("MIMO_API_KEY", "");

  // Register callback BEFORE start (like Unity does).
  SetCallback((void*)&onMessage);

  if (Start() != 0) { printf("FAIL: start\n"); return 1; }
  Sleep(1500);

  if (IsRunning() != 1) { printf("FAIL: not running\n"); return 1; }
  printf("  outbound messages received: %d\n", g_outbound.load());
  if (g_outbound.load() < 1) { printf("FAIL: no greeting callback fired\n"); return 1; }
  if (g_lastOutbound.find("greeting") == std::string::npos) {
    printf("  last outbound: %s\n", g_lastOutbound.c_str());
    printf("FAIL: greeting not seen\n"); return 1;
  }

  // Feed inbound messages like Unity does (state change -> start/stop recording).
  int rc1 = SendJson("{\"type\":\"state_change\",\"payload\":{\"state\":\"active\"},\"id\":\"s1\"}");
  int rc2 = SendJson("{\"type\":\"state_change\",\"payload\":{\"state\":\"standby\"},\"id\":\"s2\"}");
  printf("  SendJson active=%d standby=%d\n", rc1, rc2);
  if (rc1 != 1 || rc2 != 1) { printf("FAIL: SendJson rejected\n"); return 1; }

  // ScreenshotResponse fast-path: send with an id that has no pending request;
  // must be accepted without error (it just won't find a matching promise).
  int rc3 = SendJson("{\"type\":\"screenshot_response\",\"payload\":{\"path\":\"x.png\"},\"id\":\"nope\"}");
  if (rc3 != 1) { printf("FAIL: SendJson screenshot_response rejected\n"); return 1; }

  // Malformed JSON must be rejected gracefully (no crash).
  int rc4 = SendJson("{not json");
  if (rc4 != 0) { printf("FAIL: malformed JSON should be rejected\n"); return 1; }

  // Heartbeat is accepted and ignored.
  int rc5 = SendJson("{\"type\":\"heartbeat\",\"payload\":{},\"id\":\"hb\"}");
  if (rc5 != 1) { printf("FAIL: heartbeat rejected\n"); return 1; }

  Sleep(500);
  Stop();
  SetCallback(nullptr);
  FreeLibrary(dll);

  printf("ALL IN-PROCESS TRANSPORT TESTS PASSED\n");
  return 0;
}
