#pragma once
#include <atomic>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

namespace aoi {

// Plays 16-bit PCM through the default output device using winmm waveOut
// (mirrors the JS player.ts which compiles a C# player using the same API).
// Playback is synchronous; stopPlayback() interrupts an in-flight play.
void playPcm16(const std::vector<uint8_t>& pcm, int sampleRate = 24000);
void stopPlayback();
bool isPlaying();

} // namespace aoi
