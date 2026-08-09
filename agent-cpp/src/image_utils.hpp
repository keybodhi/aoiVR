#pragma once
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace aoi {

// Mirrors the JS processImage()/resizeImage() behavior. Decodes a PNG/JPEG
// image, optionally resizes to fit max dimensions and encoded (base64) size,
// and re-encodes to PNG/JPEG. Returns the result object:
//   { ok:true, data (base64), mimeType } or { ok:false, message }.
struct ImageResizeOptions {
  bool autoResizeImages = true;
  int maxWidth = 2000;
  int maxHeight = 2000;
  size_t maxBytes = static_cast<size_t>(4.5 * 1024 * 1024);
  int jpegQuality = 80;
};

struct ProcessImageResult {
  bool ok = false;
  std::string data;      // base64-encoded bytes
  std::string mimeType;  // e.g. "image/jpeg"
  std::string message;   // on !ok
  int width = 0;
  int height = 0;
  bool wasResized = false;
};

// Encode raw RGBA pixels to a base64 data URL part ready for the LLM.
ProcessImageResult processImage(const std::vector<uint8_t>& bytes,
                                const std::string& mimeType,
                                const ImageResizeOptions& opts = {});

} // namespace aoi
