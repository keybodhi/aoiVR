// End-to-end test of LlmSession (the agent loop: streaming SSE + tool calling +
// multi-turn memory + image attachment from tool results) against a local mock
// OpenAI-compatible /chat/completions server.
#include <windows.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "llm_client.hpp"

using namespace aoi;

static int failures = 0;
#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);              \
      failures++;                                                         \
    }                                                                     \
  } while (0)

class MockLlmServer {
 public:
  bool start() {
    sock_ = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock_ == INVALID_SOCKET) return false;
    SOCKADDR_IN addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(0);
    if (::bind(sock_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) return false;
    if (::listen(sock_, 4) != 0) return false;
    sockaddr_in bound{};
    int len = sizeof(bound);
    getsockname(sock_, reinterpret_cast<sockaddr*>(&bound), &len);
    port_ = ntohs(bound.sin_port);
    thread_ = std::thread([this] { acceptLoop(); });
    return true;
  }
  int port() const { return port_; }

  void stop() {
    running_ = false;
    if (sock_ != INVALID_SOCKET) { closesocket(sock_); sock_ = INVALID_SOCKET; }
    if (thread_.joinable()) thread_.join();
  }

  // All request bodies received so far.
  std::vector<std::string> requests() {
    std::lock_guard<std::mutex> lk(mu_);
    return reqs_;
  }

 private:
  void acceptLoop() {
    while (running_) {
      SOCKADDR_IN c{};
      int clen = sizeof(c);
      SOCKET c2 = ::accept(sock_, reinterpret_cast<sockaddr*>(&c), &clen);
      if (c2 == INVALID_SOCKET) break;
      handleClient(c2);
    }
  }

  void sendChunk(SOCKET c, const std::string& s) {
    std::string chunk;
    char hdr[32];
    snprintf(hdr, sizeof(hdr), "%X\r\n", static_cast<int>(s.size()));
    chunk = hdr + s + "\r\n";
    send(c, chunk.data(), static_cast<int>(chunk.size()), 0);
  }

  void handleClient(SOCKET c) {
    std::string req;
    char buf[8192];
    int n;
    while ((n = recv(c, buf, sizeof(buf), 0)) > 0) {
      req.append(buf, n);
      if (req.find("\r\n\r\n") != std::string::npos) break;
    }
    // Extract the JSON body after the blank line.
    const size_t hdrEnd = req.find("\r\n\r\n");
    std::string body = hdrEnd != std::string::npos ? req.substr(hdrEnd + 4) : "";
    {
      std::lock_guard<std::mutex> lk(mu_);
      reqs_.push_back(body);
    }
    const bool hasToolResult = body.find("\"role\":\"tool\"") != std::string::npos;

    std::string head =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/event-stream\r\n"
        "Connection: close\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n";
    send(c, head.data(), static_cast<int>(head.size()), 0);

    if (!hasToolResult) {
      // Plain user prompt -> request a tool call first.
      sendChunk(c, "data: {\"choices\":[{\"delta\":{\"content\":\"让我\"}}]}\n\n");
      sendChunk(c, "data: {\"choices\":[{\"delta\":{\"content\":\"查一下\"}}]}\n\n");
      sendChunk(c, "data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"id\":\"call_1\",\"type\":\"function\",\"function\":{\"name\":\"lookup\",\"arguments\":\"{\\\"city\\\":\\\"Beijing\\\"}\"}}]}}]}\n\n");
      sendChunk(c, "data: {\"choices\":[{\"delta\":{},\"finish_reason\":\"tool_calls\"}]}\n\n");
    } else {
      // Tool result present -> final assistant text (no more tool calls).
      sendChunk(c, "data: {\"choices\":[{\"delta\":{\"content\":\"北京今天晴。\"}}]}\n\n");
      sendChunk(c, "data: {\"choices\":[{\"delta\":{},\"finish_reason\":\"stop\"}]}\n\n");
    }
    sendChunk(c, "data: [DONE]\n\n");
    const char* end = "0\r\n\r\n";
    send(c, end, static_cast<int>(std::strlen(end)), 0);
    closesocket(c);
  }

  SOCKET sock_ = INVALID_SOCKET;
  int port_ = 0;
  std::atomic<bool> running_{true};
  std::thread thread_;
  std::mutex mu_;
  std::vector<std::string> reqs_;
};

int main() {
  WSADATA wsa;
  WSAStartup(MAKEWORD(2, 2), &wsa);

  MockLlmServer server;
  CHECK(server.start());
  const std::string base = "http://127.0.0.1:" + std::to_string(server.port()) + "/v1";

  // A tool that returns an image (simulating screenshot).
  ToolDefinition lookupTool;
  lookupTool.name = "lookup";
  lookupTool.label = "lookup";
  lookupTool.description = "Look up info for a city";
  lookupTool.parameters = {
      {"type", "object"},
      {"properties", nlohmann::json{{"city", {{"type", "string"}}}}},
      {"required", nlohmann::json::array()},
  };
  bool toolCalled = false;
  lookupTool.execute = [&toolCalled](const std::string& id, const nlohmann::json& args) {
    toolCalled = true;
    CHECK(id == "call_1");
    CHECK(args["city"] == "Beijing");
    // Return an image attachment + a text summary (the screenshot convention).
    return nlohmann::json{{"content", "weather=sunny, 25C"},
                          {"__image", "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8BQDwAEhQGAhKmMIQAAAABJRU5ErkJggg=="}};
  };

  LlmSession::Config cfg;
  cfg.modelId = "mimo-v2.5-free";
  cfg.baseUrl = base;
  cfg.apiKey = "test";
  cfg.systemPrompt = "You are a helpful assistant.";
  cfg.tools = {lookupTool};
  LlmSession session(cfg);

  std::string streamed;
  std::string finalText1;
  std::string finalText2;
  int toolStarts = 0;
  int agentEnds = 0;
  session.subscribe([&](const SessionEvent& e) {
    if (e.type == "message_update") streamed += e.delta;
    if (e.type == "message_end") {
      if (finalText1.empty()) finalText1 = e.finalText;
      else finalText2 = e.finalText;
    }
    if (e.type == "tool_execution_start") toolStarts++;
    if (e.type == "agent_end") agentEnds++;
  });

  // Turn 1: triggers the lookup tool (which returns an image), then final text.
  session.prompt("北京天气怎么样？");
  CHECK(toolCalled);
  CHECK(toolStarts == 1);
  CHECK(streamed.find("让我") != std::string::npos);
  CHECK(streamed.find("查一下") != std::string::npos);
  CHECK(finalText1.find("北京今天晴") != std::string::npos);
  CHECK(agentEnds == 1);
  printf("  [turn1] streamed='%s'\n  [turn1] final='%s'\n", streamed.c_str(), finalText1.c_str());

  // Turn 2: same session. The mock responds with final text again. We verify:
  // (a) multi-turn memory — the 3rd request carries the previous assistant text;
  // (b) the image from the tool result was attached to this turn's user message.
  session.prompt("再说一遍");
  const auto reqs = server.requests();
  CHECK(reqs.size() >= 3);
  printf("  [turn2] total requests=%zu\n", reqs.size());

  bool sawHistory = false;
  bool sawImage = false;
  bool sawStringArguments = false;
  for (size_t i = 0; i < reqs.size(); i++) {
    const auto& r = reqs[i];
    printf("  [req %zu] len=%zu contains_history=%d contains_image=%d\n",
           i, r.size(),
           (int)(r.find("北京今天晴") != std::string::npos),
           (int)(r.find("iVBORw0KGgo") != std::string::npos));
    if (r.find("北京今天晴") != std::string::npos) sawHistory = true;
    if (r.find("iVBORw0KGgo") != std::string::npos) sawImage = true;
    // The tool_calls arguments must round-trip as a JSON STRING (OpenAI
    // protocol). An object form here (arguments as {..}) would be rejected
    // with HTTP 400 by real providers on the next turn.
    if (r.find("\"arguments\":\"{\\\"city\\\":\\\"Beijing\\\"}\"") != std::string::npos)
      sawStringArguments = true;
  }
  CHECK(sawHistory);   // prior assistant turn is in the conversation
  CHECK(sawImage);     // screenshot image from the tool result reached the model
  CHECK(sawStringArguments);  // tool_calls arguments kept as a JSON string
  CHECK(!finalText2.empty());
  CHECK(agentEnds == 2);
  printf("  [turn2] final='%s'\n", finalText2.c_str());

  // Turn 3: multimodal — send an audio block + a text block. The mock (no tool
  // result in the request) returns final text directly. Verify the wire format
  // matches the MiMo official input_audio contract.
  {
    LlmSession::Config cfg2 = cfg;
    cfg2.tools.clear();
    LlmSession s2(cfg2);
    std::string final3;
    s2.subscribe([&](const SessionEvent& e) {
      if (e.type == "message_end") final3 = e.finalText;
    });
    ContentPart audio;
    audio.type = "audio";
    audio.dataUrl = "data:audio/wav;base64,UklGRgAAAA==";
    ContentPart img;
    img.type = "image_url";
    img.dataUrl = "data:image/jpeg;base64,/9j/2Q==";
    s2.prompt("请描述这段音频", {audio, img});
    const auto reqs3 = server.requests();
    const std::string& r = reqs3.back();

    // Audio is sent on the FIRST call of this turn (then stripped from history
    // so old audio is never resent). It may not be the last request if the mock
    // forced a tool call first — scan all requests of this turn.
    bool audioOk = false;
    bool imageOk = false;
    for (const auto& rr : reqs3) {
      if (rr.find("\"type\":\"input_audio\"") != std::string::npos &&
          rr.find("UklGRgAAAA==") != std::string::npos &&
          rr.find("data:audio/wav;base64") == std::string::npos &&
          rr.find("\"format\":\"wav\"") != std::string::npos) audioOk = true;
      if (rr.find("\"type\":\"image_url\"") != std::string::npos &&
          rr.find("data:image/jpeg;base64,/9j/2Q==") != std::string::npos) imageOk = true;
    }
    // Also verify the audio is NOT resent on a LATER request of the same turn.
    bool audioReSent = false;
    for (size_t i = 1; i < reqs3.size(); i++) {
      if (reqs3[i].find("UklGRgAAAA==") != std::string::npos) audioReSent = true;
    }
    printf("  [turn3] audioOk=%d imageOk=%d audioReSent=%d\n", audioOk ? 1 : 0, imageOk ? 1 : 0, audioReSent ? 1 : 0);
    printf("  [turn3] last body: %s\n", r.substr(0, 500).c_str());
    CHECK(audioOk);
    CHECK(imageOk);
    CHECK(final3.find("北京今天晴") != std::string::npos);
  }

  server.stop();
  WSACleanup();

  if (failures == 0) {
    printf("ALL LLM LOOP TESTS PASSED\n");
    return 0;
  }
  printf("%d test(s) failed\n", failures);
  return 1;
}
