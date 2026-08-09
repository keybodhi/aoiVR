#pragma once
#include <string>
#include <vector>

namespace aoi {

// RFC 4648 base64. Self-contained reimplementation (see THIRD_PARTY_NOTICES.md).
std::string base64Encode(const unsigned char* data, size_t len);
inline std::string base64Encode(const std::vector<uint8_t>& data) {
  return base64Encode(data.data(), data.size());
}
inline std::string base64Encode(const std::string& s) {
  return base64Encode(reinterpret_cast<const unsigned char*>(s.data()), s.size());
}

// Returns false on invalid input. Accepts standard and URL-safe alphabets and
// tolerates missing padding.
bool base64Decode(const std::string& in, std::vector<uint8_t>& out);

} // namespace aoi
