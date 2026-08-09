#include "protocol.hpp"

#include <cstring>

#include <nlohmann/json.hpp>

namespace aoi {

std::vector<uint8_t> encodeMessage(const Message& msg) {
  const auto json = msg.toJson().dump();
  std::vector<uint8_t> out(4 + json.size());
  const uint32_t len = static_cast<uint32_t>(json.size());
  out[0] = len & 0xFF;
  out[1] = (len >> 8) & 0xFF;
  out[2] = (len >> 16) & 0xFF;
  out[3] = (len >> 24) & 0xFF;
  std::memcpy(out.data() + 4, json.data(), json.size());
  return out;
}

bool decodeMessage(const std::vector<uint8_t>& data, Message& out) {
  try {
    const auto j = nlohmann::json::parse(data.begin(), data.end());
    out = Message::fromJson(j);
    return true;
  } catch (...) {
    return false;
  }
}

int tryReadMessage(const std::vector<uint8_t>& buf, Message& out) {
  if (buf.size() < 4) return 0;
  const uint32_t len =
      static_cast<uint32_t>(buf[0]) | (static_cast<uint32_t>(buf[1]) << 8) |
      (static_cast<uint32_t>(buf[2]) << 16) | (static_cast<uint32_t>(buf[3]) << 24);
  if (len == 0 || len > MAX_MESSAGE_BYTES) return -1;
  if (buf.size() < 4 + len) return 0;
  std::vector<uint8_t> data(buf.begin() + 4, buf.begin() + 4 + len);
  if (!decodeMessage(data, out)) return -1;
  return 4 + static_cast<int>(len);
}

} // namespace aoi
