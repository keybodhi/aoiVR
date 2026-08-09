#include "sherpa_asr.hpp"

#include <algorithm>
#include <cmath>
#include <mutex>
#include <thread>

#ifdef AOI_USE_SHERPA
#include "sherpa-onnx/c-api/c-api.h"
#endif

namespace aoi {

namespace {

// RMS energy below this is treated as silence (16k mono).
float rms(const std::vector<float>& pcm) {
  if (pcm.empty()) return 0;
  double sum = 0;
  for (float v : pcm) sum += static_cast<double>(v) * v;
  return static_cast<float>(std::sqrt(sum / pcm.size()));
}

// Infer source language from the zh-en bilingual recognizer's output.
std::string inferLanguage(const std::string& text) {
  for (unsigned char c : text) {
    if (c >= 0xE4) return "zh";  // rough CJK check (common UTF-8 lead bytes)
  }
  return "en";
}

#ifdef AOI_USE_SHERPA

struct RecognizerState {
  const SherpaOnnxOnlineRecognizer* rec = nullptr;
  std::once_flag initOnce;
  std::mutex chainMutex;
  // Simple chain: serialize transcriptions like the JS transcribeChain.
  std::vector<std::thread::id> active;
  std::recursive_mutex chain;
};

RecognizerState& state() {
  static RecognizerState s;
  return s;
}

const SherpaOnnxOnlineRecognizer* getRecognizer() {
  auto& s = state();
  // std::call_once guarantees a single recognizer creation even when awareness
  // and the interpretation segmenter transcribe concurrently from different
  // threads — no double-create, no pointer write race, no 1-2s blocking stall
  // for the second caller (it waits, then reuses the same pointer).
  std::call_once(s.initOnce, [&s]() {
    const char* modelDir = getenv("SHERPA_ASR_MODEL");
    std::string dir = modelDir ? modelDir
                               : "models/sherpa/sherpa-onnx-streaming-zipformer-small-bilingual-zh-en-2023-02-16";
    const std::string encoder = dir + "/encoder-epoch-99-avg-1.onnx";
    const std::string decoder = dir + "/decoder-epoch-99-avg-1.onnx";
    const std::string joiner = dir + "/joiner-epoch-99-avg-1.onnx";
    const std::string tokens = dir + "/tokens.txt";

    const int numThreads = std::max(1, std::min(4, static_cast<int>(std::thread::hardware_concurrency()) / 2));

    SherpaOnnxOnlineRecognizerConfig cfg{};
    cfg.feat_config.sample_rate = 16000;
    cfg.feat_config.feature_dim = 80;
    cfg.model_config.transducer.encoder = encoder.c_str();
    cfg.model_config.transducer.decoder = decoder.c_str();
    cfg.model_config.transducer.joiner = joiner.c_str();
    cfg.model_config.tokens = tokens.c_str();
    cfg.model_config.provider = "cpu";
    cfg.model_config.num_threads = numThreads;
    cfg.model_config.debug = 0;
    cfg.decoding_method = "greedy_search";
    cfg.max_active_paths = 4;
    cfg.enable_endpoint = 0;
    cfg.rule1_min_trailing_silence = 2.4;
    cfg.rule2_min_trailing_silence = 1.2;
    cfg.rule3_min_utterance_length = 20;

    s.rec = SherpaOnnxCreateOnlineRecognizer(&cfg);
  });
  return s.rec;
}

#endif // AOI_USE_SHERPA

} // namespace

SherpaResult sherpaTranscribeFull(const std::vector<float>& pcmf32) {
  // Skip silence up front.
  if (rms(pcmf32) < 0.005f) return {"", "silence"};

#ifndef AOI_USE_SHERPA
  return {"", "unavailable"};  // built without sherpa-onnx
#else
  static constexpr int SAMPLE_RATE = 16000;
  static constexpr float LEFT_PADDING_SECONDS = 0.3f;
  static constexpr float TAIL_PADDING_SECONDS = 0.6f;

  auto& s = state();
  const SherpaOnnxOnlineRecognizer* rec = getRecognizer();
  if (!rec) return {"", "unavailable"};

  // Serialize (mirrors the JS transcribeChain).
  std::lock_guard<std::recursive_mutex> lock(s.chain);

  SherpaOnnxOnlineStream* stream = SherpaOnnxCreateOnlineStream(rec);
  if (!stream) return {"", "unavailable"};

  const int leftPad = static_cast<int>(SAMPLE_RATE * LEFT_PADDING_SECONDS);
  const int tailPad = static_cast<int>(SAMPLE_RATE * TAIL_PADDING_SECONDS);
  std::vector<float> padded(leftPad + pcmf32.size() + tailPad, 0.0f);
  std::copy(pcmf32.begin(), pcmf32.end(), padded.begin() + leftPad);

  SherpaOnnxOnlineStreamAcceptWaveform(stream, SAMPLE_RATE, padded.data(), static_cast<int>(padded.size()));
  SherpaOnnxOnlineStreamInputFinished(stream);
  while (SherpaOnnxIsOnlineStreamReady(rec, stream)) {
    SherpaOnnxDecodeOnlineStream(rec, stream);
  }
  const char* result = SherpaOnnxGetOnlineStreamResult(rec, stream);
  std::string text = result ? result : "";
  SherpaOnnxOnlineStreamClear(stream);
  SherpaOnnxDestroyOnlineStream(stream);

  // Trim whitespace.
  size_t b = 0, e = text.size();
  while (b < e && (text[b] == ' ' || text[b] == '\t' || text[b] == '\n')) b++;
  while (e > b && (text[e - 1] == ' ' || text[e - 1] == '\t' || text[e - 1] == '\n')) e--;
  text = text.substr(b, e - b);

  if (text.empty()) return {"", "auto"};
  return {text, inferLanguage(text)};
#endif
}

} // namespace aoi
