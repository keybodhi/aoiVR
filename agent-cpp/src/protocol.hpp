#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "types.hpp"

namespace aoi {

// Encode a message as [4-byte LE length][UTF-8 JSON]. Mirrors protocol.ts.
std::vector<uint8_t> encodeMessage(const Message& msg);

// Decode the JSON payload of a single message (without the 4-byte header).
// Returns false if the JSON is malformed.
bool decodeMessage(const std::vector<uint8_t>& data, Message& out);

// Parse one length-prefixed message from a byte buffer. Returns the consumed
// byte count, or 0 if more data is needed, or -1 on protocol error.
// The buffer may contain multiple messages back-to-back.
int tryReadMessage(const std::vector<uint8_t>& buf, Message& out);

constexpr int64_t MAX_MESSAGE_BYTES = 64LL * 1024 * 1024;

} // namespace aoi
