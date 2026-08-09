// End-to-end test of the SSE streaming HTTP client against a local mock server.
// Validates chunked SSE parsing used by both the MiMo TTS and the LLM client.
#include <windows.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include "base64.hpp"
#include "http_client.hpp"
#include "tts.hpp"

using namespace aoi;

static int failures = 0;
#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);              \
      failures++;                                                         \
    }                                                                     \
  } while (0)

// A minimal HTTP/1.1 server that streams SSE frames with a delay, then closes.
class MockSseServer {
 public:
  bool start() {
    sock_ = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock_ == INVALID_SOCKET) return false;
    SOCKADDR_IN addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(port_);
    if (::bind(sock_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) return false;
    if (::listen(sock_, 1) != 0) return false;
    sockaddr_in bound{};
    int len = sizeof(bound);
    getsockname(sock_, reinterpret_cast<sockaddr*>(&bound), &len);
    port_ = ntohs(bound.sin_port);
    thread_ = std::thread([this] { acceptLoop(); });
    return true;
  }

  int port() const { return port_; }
  int connections() const { return connections_; }
  void stop() {
    running_ = false;
    if (sock_ != INVALID_SOCKET) {
      closesocket(sock_);
      sock_ = INVALID_SOCKET;
    }
    if (thread_.joinable()) thread_.join();
  }

 private:
  void acceptLoop() {
    while (running_) {
      SOCKADDR_IN c{};
      int clen = sizeof(c);
      SOCKET c2 = ::accept(sock_, reinterpret_cast<sockaddr*>(&c), &clen);
      if (c2 == INVALID_SOCKET) break;
      connections_++;
      printf("  [mock] accepted connection #%d\n", connections_);
      handleClient(c2);
    }
  }

  void handleClient(SOCKET c) {
    // Read request (headers up to blank line).
    std::string req;
    char buf[1024];
    int n;
    while ((n = recv(c, buf, sizeof(buf), 0)) > 0) {
      req.append(buf, n);
      if (req.find("\r\n\r\n") != std::string::npos) break;
    }
    // Respond with chunked SSE.
    std::string head =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/event-stream\r\n"
        "Connection: close\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n";
    send(c, head.data(), static_cast<int>(head.size()), 0);

    const char* events[] = {
        "data: {\"choices\":[{\"delta\":{\"content\":\"你\"}}]}\n\n",
        "data: {\"choices\":[{\"delta\":{\"content\":\"好\"}}]}\n\n",
        "data: {\"choices\":[{\"delta\":{\"audio\":{\"data\":\"Zm9vYmFy\"}}}]}\n\n",
        "data: [DONE]\n\n",
    };
    for (const char* e : events) {
      if (!running_) break;
      const int l = static_cast<int>(std::strlen(e));
      std::string chunk;
      char hdr[32];
      snprintf(hdr, sizeof(hdr), "%X\r\n", l);
      chunk = hdr;
      chunk += e;
      chunk += "\r\n";
      send(c, chunk.data(), static_cast<int>(chunk.size()), 0);
      Sleep(20);
    }
    // Terminating chunk.
    const char* end = "0\r\n\r\n";
    send(c, end, static_cast<int>(std::strlen(end)), 0);
    closesocket(c);
  }

  SOCKET sock_ = INVALID_SOCKET;
  int port_ = 0;
  int connections_ = 0;
  std::atomic<bool> running_{true};
  std::thread thread_;
};

int main() {
  WSADATA wsa;
  WSAStartup(MAKEWORD(2, 2), &wsa);

  MockSseServer server;
  CHECK(server.start());
  if (server.port() == 0) {
    printf("server failed to start\n");
    return 1;
  }
  const std::string url = "http://127.0.0.1:" + std::to_string(server.port()) + "/v1/chat/completions";

  // Raw HttpClient test first: confirm body bytes arrive.
  HttpClient http;
  std::string raw;
  const auto r1 = http.postStream(url, {"Content-Type: application/json"}, "{}",
                                  [&](const char* d, size_t len) {
                                    raw.append(d, len);
                                  });
  printf("  [raw] status=%ld raw=%zu bytes\n", r1.status, raw.size());
  CHECK(r1.status == 200);
  CHECK(raw.find("data: ") == 0);  // SSE stream starts correctly

  // Drive through MiMoTTS.speak which contains the real SSE parser.
  TtsConfig cfg;
  cfg.apiKey = "test";
  cfg.baseUrl = "http://127.0.0.1:" + std::to_string(server.port()) + "/v1";
  MiMoTTS tts(cfg);
  std::vector<std::string> chunks;
  const bool ok = tts.speak("hello", "", [&](const TtsChunk& c) {
    std::vector<uint8_t> dec;
    CHECK(base64Decode(c.base64, dec));
    CHECK(dec.size() == 6);  // "foobar"
    chunks.push_back(c.base64);
    printf("  [tts] chunk #%d decoded=%zu bytes\n", c.index, dec.size());
  });
  printf("  tts.speak ok=%d chunks=%zu\n", ok ? 1 : 0, chunks.size());
  CHECK(ok);
  CHECK(chunks.size() == 1);  // only the audio data: line

  server.stop();
  WSACleanup();

  if (failures == 0) {
    printf("ALL SSE TESTS PASSED\n");
    return 0;
  }
  printf("%d test(s) failed\n", failures);
  return 1;
}
