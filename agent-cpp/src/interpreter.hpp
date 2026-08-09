#pragma once
#include <functional>
#include <string>

#include "speech_segmenter.hpp"

namespace aoi {

struct TranslationSegment : SpeechSegment {};

struct SpeechInterpreterOptions {
  std::function<void(const SpeechSegment&)> onSegment;
  std::function<void(const std::string& state, const std::string& msg)> onState;
  // Sliding-window params for interpretation: block W seconds, overlap O ms.
  // Every (W*1000-O) ms a window of the latest W seconds is sent to the audio
  // LLM. 4s / 1500ms = good balance (short sentences ~3s, long speech ~5s).
  int windowSeconds = 4;
  int overlapMs = 1500;
};

// Simultaneous interpretation: captures system speaker audio with a sliding
// window (W=4s block, O=1.5s overlap, step 2.5s) and emits overlapping audio
// segments that the agent translates with an audio-capable LLM. Mirrors
// interpreter.ts (sliding-window design).
class SpeechInterpreter {
 public:
  explicit SpeechInterpreter(SpeechInterpreterOptions opts);
  ~SpeechInterpreter();

  bool running() const;

  bool start();
  void stop();
  void abort();

 private:
  SpeechInterpreterOptions opts_;
  SpeechSegmenter segmenter_;
};

} // namespace aoi
