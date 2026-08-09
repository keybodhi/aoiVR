// Standalone smoke tests for the pure modules (protocol, base64, agent-utils,
// audio-utils, image-utils). No LLM/pipe/audio hardware needed.
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "agent_utils.hpp"
#include "audio_utils.hpp"
#include "base64.hpp"
#include "image_utils.hpp"
#include "protocol.hpp"

using namespace aoi;

static int failures = 0;
#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);              \
      failures++;                                                         \
    }                                                                     \
  } while (0)

int main() {
  // ---- base64 ----
  {
    const std::string in = "hello, world \x01\x02\x03";
    const auto b64 = base64Encode(reinterpret_cast<const unsigned char*>(in.data()), in.size());
    std::vector<uint8_t> out;
    CHECK(base64Decode(b64, out));
    CHECK(std::string(out.begin(), out.end()) == in);
    // RFC 4648 test vectors
    CHECK(base64Encode((const unsigned char*)"", 0) == "");
    CHECK(base64Encode((const unsigned char*)"f", 1) == "Zg==");
    CHECK(base64Encode((const unsigned char*)"fo", 2) == "Zm8=");
    CHECK(base64Encode((const unsigned char*)"foo", 3) == "Zm9v");
    CHECK(base64Encode((const unsigned char*)"foob", 4) == "Zm9vYg==");
    CHECK(base64Encode((const unsigned char*)"fooba", 5) == "Zm9vYmE=");
    CHECK(base64Encode((const unsigned char*)"foobar", 6) == "Zm9vYmFy");
  }

  // ---- protocol round trip ----
  {
    Message m;
    m.type = MessageType::UserInput;
    m.payload = nlohmann::json{{"text", "你好"}};
    m.timestamp = "2026-08-05T00:00:00Z";
    m.id = "abc123";
    const auto bytes = encodeMessage(m);
    CHECK(bytes.size() > 4);
    Message out;
    const int consumed = tryReadMessage(bytes, out);
    CHECK(consumed == static_cast<int>(bytes.size()));
    CHECK(out.type == MessageType::UserInput);
    CHECK(out.payload["text"] == "你好");
    CHECK(out.id == "abc123");
  }

  // ---- protocol: bad length / need-more-data / multi-message buffer ----
  {
    // Zero length -> protocol error.
    std::vector<uint8_t> bad = {0, 0, 0, 0};
    Message out;
    CHECK(tryReadMessage(bad, out) == -1);
    // Too-large length -> protocol error.
    std::vector<uint8_t> huge = {0x00, 0x00, 0x00, 0x05};  // > 64MB (0x05000000)
    CHECK(tryReadMessage(huge, out) == -1);
    // Truncated header -> need more data.
    std::vector<uint8_t> head2 = {0x05, 0x00};
    CHECK(tryReadMessage(head2, out) == 0);
    // Header present but body incomplete -> need more data.
    Message m;
    m.type = MessageType::Heartbeat;
    m.payload = nlohmann::json::object();
    m.id = "x";
    m.timestamp = "t";
    auto full = encodeMessage(m);
    std::vector<uint8_t> partial(full.begin(), full.begin() + 4 + 2);
    CHECK(tryReadMessage(partial, out) == 0);

    // Two messages back-to-back in one buffer parse sequentially.
    Message m2;
    m2.type = MessageType::StateChange;
    m2.payload = nlohmann::json{{"state", "active"}};
    m2.id = "y";
    m2.timestamp = "t2";
    auto a = encodeMessage(m);
    auto b = encodeMessage(m2);
    std::vector<uint8_t> both;
    both.insert(both.end(), a.begin(), a.end());
    both.insert(both.end(), b.begin(), b.end());
    Message o1, o2;
    const int c1 = tryReadMessage(both, o1);
    CHECK(c1 > 0);
    CHECK(o1.type == MessageType::Heartbeat);
    std::vector<uint8_t> rest(both.begin() + c1, both.end());
    const int c2 = tryReadMessage(rest, o2);
    CHECK(c2 > 0);
    CHECK(o2.type == MessageType::StateChange);
    CHECK(o2.payload["state"] == "active");
  }

  // ---- protocol: malformed JSON body -> decode failure ----
  {
    std::vector<uint8_t> data = {'{', 'n', 'o', 't', 'j', 's', 'o', 'n'};
    Message out;
    CHECK(!decodeMessage(data, out));
  }

  // ---- types: all 20 message types round-trip ----
  {
    const char* names[] = {
        "greeting", "user_input", "assistant_response", "system_command",
        "screenshot_request", "screenshot_response", "error", "state_change",
        "acknowledge", "heartbeat", "tui_feed", "tui_resize", "tui_clear",
        "tui_scroll", "tui_info", "translation", "interpretation_state",
        "awareness_on", "awareness_off", "tts_stop"};
    for (const char* n : names) {
      const MessageType t = messageTypeFromString(n);
      CHECK(t != MessageType::Acknowledge || std::string(n) == "acknowledge");
      CHECK(std::string(messageTypeToString(t)) == n);
    }
    // Unknown string -> Acknowledge.
    CHECK(messageTypeFromString("no_such_type") == MessageType::Acknowledge);
  }

  // ---- audio_utils ----
  {
    std::vector<uint8_t> pcm;
    for (int i = 0; i < 16000; i++) {
      const int16_t s = static_cast<int16_t>(i % 1000);
      pcm.push_back(static_cast<uint8_t>(s & 0xFF));
      pcm.push_back(static_cast<uint8_t>((s >> 8) & 0xFF));
    }
    const auto f = pcm16ToFloat32(pcm.data(), pcm.size());
    CHECK(f.size() == 16000);
    const auto back = float32ToPcm16(f.data(), f.size());
    CHECK(back.size() == pcm.size());
    const auto wav = buildWavFromPcm(pcm);
    CHECK(wav.size() == pcm.size() + 44);
    CHECK(std::string(wav.begin(), wav.begin() + 4) == "RIFF");
    CHECK(std::string(wav.begin() + 8, wav.begin() + 12) == "WAVE");

    FloatRingBuffer ring;
    ring.push(f.data(), 1000);
    CHECK(ring.length() == 1000);
    const auto chunk = ring.take(512);
    CHECK(chunk.size() == 512);
    CHECK(ring.length() == 488);
  }

  // ---- agent_utils ----
  {
    CHECK(sanitizeForTts("Hello **world**!") == "Hello world!");
    CHECK(sanitizeForTts("<thinking>ignore this</thinking> real text") == "real text");
    CHECK(isTtsJunk("\"\""));
    CHECK(!isTtsJunk("hello"));
    CHECK(sanitizeForTts("`code` and *x*") == "code and x");

    std::vector<std::string> hist = {"翻译1", "翻译2"};
    const auto prefix = buildContextPrefix(hist);
    CHECK(prefix.find("翻译1") != std::string::npos);
    CHECK(buildContextPrefix({}) == "");
  }

  // ---- splitSentences (UTF-8 CJK punctuation) ----
  {
    const auto parts = splitSentences("你好。世界！如何?结束.");
    CHECK(parts.size() == 4);
    CHECK(parts[0] == "你好。");
    CHECK(parts[1] == "世界！");
    CHECK(parts[2] == "如何?");
    CHECK(parts[3] == "结束.");
    const auto noEnd = splitSentences("没有标点");
    CHECK(noEnd.size() == 1);
    CHECK(noEnd[0] == "没有标点");
  }

  // ---- image_utils (decode a small BMP, resize it) ----
  {
    // Build a 100x50 24-bit BMP (bottom-up) with red pixels.
    const int w = 100, h = 50;
    const int rowBytes = w * 3;
    const int pad = (4 - (rowBytes % 4)) % 4;
    const int dataSize = (rowBytes + pad) * h;
    const int fileSize = 54 + dataSize;
    std::vector<uint8_t> bmp(fileSize, 0);
    bmp[0] = 'B'; bmp[1] = 'M';
    bmp[2] = fileSize & 0xFF; bmp[3] = (fileSize >> 8) & 0xFF;
    bmp[4] = (fileSize >> 16) & 0xFF; bmp[5] = (fileSize >> 24) & 0xFF;
    bmp[10] = 54;
    bmp[14] = 40;
    bmp[18] = w & 0xFF; bmp[19] = (w >> 8) & 0xFF;
    bmp[22] = h & 0xFF; bmp[23] = (h >> 8) & 0xFF;
    bmp[26] = 1;   // planes
    bmp[28] = 24;  // bpp
    for (int y = 0; y < h; y++) {
      size_t off = 54 + static_cast<size_t>(y) * (rowBytes + pad);
      for (int x = 0; x < w; x++) {
        bmp[off + x * 3 + 0] = 0;      // B
        bmp[off + x * 3 + 1] = 0;      // G
        bmp[off + x * 3 + 2] = 255;    // R
      }
    }

    // No-resize path: keep original bytes.
    ImageResizeOptions keep;
    keep.maxWidth = 2000;
    keep.maxHeight = 2000;
    const auto resKeep = processImage(bmp, "image/bmp", keep);
    CHECK(resKeep.ok);
    CHECK(!resKeep.wasResized);
    CHECK(resKeep.width == w && resKeep.height == h);

    // Resize path: force downscale.
    ImageResizeOptions opts;
    opts.maxWidth = 20;
    opts.maxHeight = 20;
    opts.maxBytes = 100000;
    const auto res = processImage(bmp, "image/bmp", opts);
    CHECK(res.ok);
    CHECK(res.wasResized);
    CHECK(res.width <= 20 && res.height <= 20);
    CHECK(res.mimeType == "image/jpeg" || res.mimeType == "image/png");

    // Garbage input -> ok=false.
    const std::vector<uint8_t> garbage = {'N', 'O', 'T', 'A', 'I', 'M', 'A', 'G', 'E'};
    const auto resBad = processImage(garbage, "image/png", keep);
    CHECK(!resBad.ok);
  }

  if (failures == 0) {
    printf("ALL TESTS PASSED\n");
    return 0;
  }
  printf("%d test(s) failed\n", failures);
  return 1;
}
