# Aoi VR Agent

> **An AI voice assistant that lives in your SteamVR hand.**

A holographic hand panel attached to your controller — driven by a local
native C++ agent with an embedded LLM client, streaming TTS, VR screenshots,
and gesture control. Talk to it, ask it to look around your VR space, or
control your system — all from the palm of your hand.

## Features

| | |
|---|---|
| 🎙️ **Voice conversation** | LLM chat (OpenAI-compatible, optimized for the opencode zen/go gateway: prompt caching, streaming reasoning, usage tracking) with streaming TTS |
| 🖐️ **SteamVR hand panel** | OpenVR overlay attached to the right controller — laser/raycast input, dashboard mirror |
| 👁️ **Environment awareness** | On-demand VR screenshots + optional continuous vision & system-audio transcription |
| 🌐 **Simultaneous interpretation** | 同声传译 — translates system audio in real time |
| 🔊 **System control** | App-level volume (Windows Volume Mixer), VR brightness dimming |
| 🧩 **Native core** | `aoi_agent.dll` (C++20) — self-contained, in-process agent loop (never blocks Unity's render thread) |

## Quick start (prebuilt release)

Download the latest release zip from the
[Releases page](https://github.com/keybodhi/aoiVR/releases), extract it, then:

```
launch.bat
```

On first run it creates `aoi_config.json` from the template — fill in your
LLM / TTS API keys (see `aoi_config.json.example`), then run again. Or launch
directly:

```
AoiVR.exe            # SteamVR overlay mode
AoiVR.exe -desktop   # desktop preview window (no headset needed)
AoiVR.exe -desktop -demo   # scripted demo mode (no API keys needed)
```

The `-demo` mode plays a simulated conversation through the panel — useful to
see the UI without any keys, and for recording/demos.

## Building from source

Requirements: Visual Studio (MSVC + CMake), Unity 6000.5 (IL2CPP Windows
Standalone), SteamVR.

### 1. Native agent DLL

```bat
cd agent-cpp
cmake -S . -B build
cmake --build build --config Release --target aoi-agent-dll
```

Alternatively `agent-cpp\gate.cmd` runs the full quality gate (configure +
build + all test suites).

### 2. Unity player

1. Copy `agent-cpp/build/Release/aoi_agent.dll` to
   `unity-client/Assets/Aoi/Plugins/x86_64/aoi_agent.dll`
2. Open `unity-client` in Unity 6000.5 and run `Build/Build Aoi` (menu), or
   headless:

```bat
Unity.exe -batchmode -nographics -quit ^
  -projectPath unity-client -executeMethod BuildScript.Build
```

Output: `unity-client/Build/AoiVR.exe`.

## Repository layout

| Path | Purpose |
|---|---|
| `agent-cpp/` | Native C++ agent (LLM client, TTS, ASR, DSP, tools) exported as `aoi_agent.dll`; system prompts in `src/prompts.hpp` |
| `unity-client/` | Unity project (SteamVR overlay UI, hand panel, agent bridge) |

## License

MIT — see [LICENSE](LICENSE). Third-party notices in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md), license texts in `licenses/`.
