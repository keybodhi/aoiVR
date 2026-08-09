#include "image_utils.hpp"

#include <algorithm>
#include <cstring>
#include <set>

#include "base64.hpp"

#include <stb_image.h>
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb_image_resize2.h>
#include <stb_image_write.h>

namespace aoi {

namespace {

std::string encodeCandidate(const std::vector<uint8_t>& bytes, const std::string& mimeType) {
  return base64Encode(bytes.data(), bytes.size());
}

// stb_write_func callback accumulating into a std::vector<uint8_t>.
void writeToVec(void* context, void* data, int size) {
  auto* v = static_cast<std::vector<uint8_t>*>(context);
  const auto* p = static_cast<const uint8_t*>(data);
  v->insert(v->end(), p, p + size);
}

} // namespace

ProcessImageResult processImage(const std::vector<uint8_t>& bytes,
                                const std::string& mimeType,
                                const ImageResizeOptions& opts) {
  ProcessImageResult fail = {false, "", "", "image decode failed", 0, 0, false};

  int w = 0, h = 0, n = 0;
  unsigned char* raw = stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()),
                                             &w, &h, &n, 4);
  if (!raw) {
    fail.message = std::string("stbi_load failed: ") + (stbi_failure_reason() ? stbi_failure_reason() : "unknown");
    return fail;
  }
  std::vector<uint8_t> rgba(raw, raw + static_cast<size_t>(w) * h * 4);
  stbi_image_free(raw);

  const size_t inputBase64Size = (bytes.size() + 2) / 3 * 4;
  const bool withinDims = w <= opts.maxWidth && h <= opts.maxHeight;
  const bool withinBytes = inputBase64Size < opts.maxBytes;

  ProcessImageResult ok;
  ok.ok = true;
  ok.width = w;
  ok.height = h;

  if (!opts.autoResizeImages || (withinDims && withinBytes)) {
    // Return the original bytes unchanged.
    ok.data = base64Encode(bytes.data(), bytes.size());
    ok.mimeType = mimeType.empty() ? "image/png" : mimeType;
    ok.wasResized = false;
    return ok;
  }

  // Compute target dimensions fitting max limits.
  int tw = w, th = h;
  if (tw > opts.maxWidth) {
    th = static_cast<int>(std::lround(static_cast<double>(th) * opts.maxWidth / tw));
    tw = opts.maxWidth;
  }
  if (th > opts.maxHeight) {
    tw = static_cast<int>(std::lround(static_cast<double>(tw) * opts.maxHeight / th));
    th = opts.maxHeight;
  }
  if (tw < 1) tw = 1;
  if (th < 1) th = 1;

  // Quality ladder mirroring opencode exactly (packages/opencode image.ts):
  // fixed [80, 85, 70, 55, 40], tried HIGH quality first — the first
  // candidate under maxBytes wins.
  const std::vector<int> qualities = {80, 85, 70, 55, 40};

  auto tryEncodings = [&](int width, int height) -> std::vector<std::pair<std::string, std::string>> {
    std::vector<uint8_t> resized(static_cast<size_t>(width) * height * 4);
    if (!stbir_resize_uint8_linear(rgba.data(), w, h, w * 4, resized.data(), width, height,
                                   width * 4, STBIR_RGBA)) {
      return {};
    }
    std::vector<std::pair<std::string, std::string>> candidates;  // (mime, base64)
    // PNG candidate.
    std::vector<uint8_t> pngBuf;
    if (stbi_write_png_to_func(writeToVec, &pngBuf, width, height, 4, resized.data(), width * 4)) {
      candidates.emplace_back("image/png", base64Encode(pngBuf.data(), pngBuf.size()));
    }
    // JPEG candidates.
    for (int q : qualities) {
      std::vector<uint8_t> jpgBuf;
      if (stbi_write_jpg_to_func(writeToVec, &jpgBuf, width, height, 4, resized.data(), q)) {
        candidates.emplace_back("image/jpeg", base64Encode(jpgBuf.data(), jpgBuf.size()));
      }
    }
    return candidates;
  };

  int cw = tw, ch = th;
  for (;;) {
    const auto candidates = tryEncodings(cw, ch);
    for (const auto& [mime, b64] : candidates) {
      if (b64.size() < opts.maxBytes) {
        ok.data = b64;
        ok.mimeType = mime;
        ok.width = cw;
        ok.height = ch;
        ok.wasResized = true;
        return ok;
      }
    }
    if (cw == 1 && ch == 1) break;
    const int nw = cw == 1 ? 1 : std::max(1, static_cast<int>(cw * 0.75));
    const int nh = ch == 1 ? 1 : std::max(1, static_cast<int>(ch * 0.75));
    if (nw == cw && nh == ch) break;
    cw = nw;
    ch = nh;
  }

  return fail;
}

} // namespace aoi
