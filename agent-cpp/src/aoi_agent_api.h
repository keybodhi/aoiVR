/*
 * Aoi agent C ABI — for Unity P/Invoke.
 *
 * The agent runs on its OWN background thread started by AoiAgent_Start(),
 * never on the Unity main thread, so rendering is never blocked.
 *
 * Messaging is IN-PROCESS (no named pipe):
 *   - Unity -> agent: AoiAgent_SendJson(json) pushes a message into the agent.
 *   - agent -> Unity: AoiAgent_SetMessageCallback(cb) registers a callback that
 *     the agent's background thread invokes with each outbound JSON message.
 * The callback must be a STATIC method (MonoPInvokeCallback) for IL2CPP.
 */
#pragma once

#ifdef _WIN32
#define AOI_API __declspec(dllexport)
#else
#define AOI_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Callback the agent invokes (from its background thread) with outbound JSON.
 * data is valid only for the duration of the call. */
typedef void (*AoiAgent_MessageCallback)(const char* json);

/* Optional log callback so the agent's internal logs reach Unity (useful for
 * debugging). NULL disables. */
typedef void (*AoiAgent_LogCallback)(const char* line);

/* Set process environment variable consumed by the agent. Safe before/after
 * AoiAgent_Start(). */
AOI_API void AoiAgent_SetEnv(const char* name, const char* value);

/* Start the agent on a background thread. Returns 0 on success, nonzero if
 * already running. Non-blocking. */
AOI_API int AoiAgent_Start(void);

/* Request the agent to stop and join its background thread. Blocking. Safe to
 * call when not running. */
AOI_API void AoiAgent_Stop(void);

/* Returns 1 while the agent thread is running, 0 otherwise. */
AOI_API int AoiAgent_IsRunning(void);

/* Register the outbound message callback. Pass NULL to unregister. The
 * callback fires on the agent's own thread; marshal to the Unity main thread
 * inside it if you touch Unity APIs. */
AOI_API void AoiAgent_SetMessageCallback(AoiAgent_MessageCallback cb);

/* Register an optional log callback (agent logs -> Unity). NULL disables. */
AOI_API void AoiAgent_SetLogCallback(AoiAgent_LogCallback cb);

/* Send a message (JSON string, same schema as the old pipe protocol) from
 * Unity into the agent. Thread-safe. Returns 1 if accepted, 0 if the agent is
 * not running. */
AOI_API int AoiAgent_SendJson(const char* json);

/* ---- Semantic senders: Unity calls these instead of building JSON. The DLL
 * builds the message envelope (type/payload/timestamp/id) internally, so the
 * protocol schema lives entirely in the DLL. All are thread-safe and return
 * 1 if accepted, 0 if the agent is not running. ---- */

/* state_change: {state, mode?, shot_path?} — Unity->agent recording control. */
AOI_API int AoiAgent_SendStateChange(const char* state, const char* mode,
                                     const char* shotPath);

/* tts_stop: user interrupted TTS. */
AOI_API int AoiAgent_SendTtsStop(void);

/* screenshot_response: {image?, path?, error?, width?, height?, format?} */
AOI_API int AoiAgent_SendScreenshotPath(const char* requestId, const char* path);
AOI_API int AoiAgent_SendScreenshotImage(const char* requestId, const char* base64Jpeg);
AOI_API int AoiAgent_SendScreenshotError(const char* requestId, const char* error);

/* vr_skill_response: raw result JSON for a vr_skill_request
 * ({skill, ...args}); the payload must be a complete JSON object
 * (e.g. {"ok":true,"message":"brightness set to 0.5"} or {"ok":false,"error":"..."}). */
AOI_API int AoiAgent_SendVrSkillResult(const char* requestId, const char* resultJson);

/* display_result: Unity ack for a display message. */
AOI_API int AoiAgent_SendDisplayResult(bool success);

#ifdef __cplusplus
}
#endif

