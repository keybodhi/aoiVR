// Stability / crash-resilience test for aoi_agent.dll.
//
// The agent runs inside the Unity process; a crash in the agent must never take
// down the host. This loads the DLL like Unity does (P/Invoke), starts the agent
// on its background thread, then:
//  1. floods it with malformed / garbage / hostile inbound JSON via SendJson;
//  2. churns the lifecycle repeatedly;
//  3. verifies the host process is still alive and responsive throughout.
//
// If any of this crashes the process, the test exits abnormally and the CI gate
// fails.
#include <windows.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>

#include "dll_abi.h"

typedef void (*SetEnvFn)(const char*, const char*);
typedef int (*StartFn)(void);
typedef void (*StopFn)(void);
typedef int (*IsRunningFn)(void);
typedef void (*SetCallbackFn)(void*);
typedef int (*SendJsonFn)(const char*);

void __stdcall onMessage(const char*) { /* ignore */ }

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
  SetCallback((void*)&onMessage);

  const std::string hugeJson(4000, 'A');
  const std::string longInput = "{\"type\":\"user_input\",\"payload\":{\"text\":\"" + std::string(2000, 'x') + "\"}}";
  const char* hostile[] = {
      "{not json",
      "{\"type\":\"no_such_type\",\"payload\":{}}",
      "{\"a\":{\"a\":{}}",
      "{\"type\":\"state_change\",\"payload\":{\"state\":\"active\"}}",
      "{\"type\":\"state_change\",\"payload\":{\"state\":\"standby\"}}",
      "{\"type\":\"tts_stop\",\"payload\":{}}",
      "{\"type\":\"screenshot_response\",\"payload\":{\"path\":\"x.png\"},\"id\":\"zzz\"}",
      "{\"type\":\"user_input\",\"payload\":{\"text\":\"hello\"}}",
      "[]",
      "\"just a string\"",
      "null",
      "12345",
      "{\"type\":\"heartbeat\",\"payload\":{}}",
  };
  // Send a few longer payloads too, but only 1x (not in the flood loop) to
  // avoid flooding the real LLM HTTP path with the empty API key.
  const char* longMessages[] = { hugeJson.c_str(), longInput.c_str() };

  int pass = 0;
  for (int i = 0; i < 3; i++) {
    printf("  [cycle %d] start\n", i); fflush(stdout);
    if (Start() != 0) { printf("FAIL: Start #%d\n", i); return 1; }
    Sleep(800);
    if (IsRunning() != 1) { printf("FAIL: not running after start #%d\n", i); return 1; }
    printf("  [cycle %d] flood\n", i); fflush(stdout);

    // Flood with hostile messages from a second thread to stress concurrency.
    std::thread flood([&] {
      for (int k = 0; k < 20; k++) {
        for (int idx = 0; idx < (int)(sizeof(hostile)/sizeof(hostile[0])); idx++) {
          SendJson(hostile[idx]);
        }
      }
      // Long payloads once per cycle (these hit the real LLM HTTP path; the
      // empty key fails fast, but repeated calls could pile up in the queue).
      SendJson(longMessages[0]);
      SendJson(longMessages[1]);
    });
    flood.join();
    printf("  [cycle %d] flood done, stop\n", i); fflush(stdout);

    Stop();
    Sleep(200);
    if (IsRunning() != 0) { printf("FAIL: still running after stop #%d\n", i); return 1; }
    printf("  [cycle %d] stopped\n", i); fflush(stdout);
    pass++;
  }

  // Final run + polling.
  if (Start() != 0) { printf("FAIL: final start\n"); return 1; }
  for (int i = 0; i < 20; i++) {
    Sleep(100);
    if (IsRunning() != 1) { printf("FAIL: agent died mid-run\n"); return 1; }
  }
  Stop();
  SetCallback(nullptr);
  FreeLibrary(dll);

  printf("  survived %d lifecycle cycles + hostile input flood\n", pass);
  printf("ALL STABILITY TESTS PASSED (host process alive)\n");
  return 0;
}
