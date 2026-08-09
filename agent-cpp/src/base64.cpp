#include "base64.hpp"

#include <base64.h>

namespace aoi {

namespace {

bool isValidBase64(const std::string& in) {
  // RFC 4648 alphabet + trailing '=' padding only. Whitespace is not accepted
  // by the underlying decoder (b64_lookup returns 255 for it, silently
  // corrupting output), so reject it here — a corrupt TTS chunk must be
  // skipped, not played as a pop.
  size_t eq = std::string::npos;
  for (size_t i = 0; i < in.size(); ++i) {
    const char c = in[i];
    const bool alpha = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                       (c >= '0' && c <= '9') || c == '+' || c == '/';
    if (c == '=') {
      if (eq == std::string::npos) eq = i;
      continue;  // allow multiple trailing '='
    }
    if (!alpha) return false;  // any non-alphabet non-padding char is invalid
    if (eq != std::string::npos) return false;  // data after '=' is invalid
  }
  if (eq != std::string::npos && in.size() - eq > 2) return false;  // max 2 padding
  return !in.empty();
}

} // namespace

std::string base64Encode(const unsigned char* data, size_t len) {
  std::string out;
  const std::string in(reinterpret_cast<const char*>(data), len);
  Base64::Encode(in, &out);
  return out;
}

bool base64Decode(const std::string& in, std::vector<uint8_t>& out) {
  if (!isValidBase64(in)) return false;
  std::string decoded;
  if (!Base64::Decode(in, &decoded)) return false;
  out.assign(decoded.begin(), decoded.end());
  return true;
}

} // namespace aoi
