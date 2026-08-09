#pragma once
#include <string>
#include <vector>

namespace aoi {

struct SherpaResult {
  std::string text;
  std::string language;  // 'zh' when CJK present, else 'en'; "silence" for silence
};

// Transcribe 16kHz mono float samples (range [-1, 1]) to text. When built
// without sherpa-onnx (AOI_USE_SHERPA), returns "" / "unavailable" so the
// pipeline degrades gracefully (mirrors the JS "sherpa error is non-fatal"
// behavior).
SherpaResult sherpaTranscribeFull(const std::vector<float>& pcmf32);

} // namespace aoi
