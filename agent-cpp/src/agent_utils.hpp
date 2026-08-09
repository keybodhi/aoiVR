#pragma once
#include <string>
#include <vector>

namespace aoi {

// Build the interpretation-history prefix injected into user prompts.
// Returns "" when there is no recent translation backlog; otherwise a capped
// snippet. Mirrors agent-utils.ts buildContextPrefix().
std::string buildContextPrefix(const std::vector<std::string>& history);

// Clean model output before TTS: strip think blocks, code fences, emoji and
// markdown punctuation, collapsing whitespace. Mirrors agent-utils.ts.
std::string sanitizeForTts(const std::string& text);

// True when a TTS candidate is pure punctuation/spacing. Mirrors isTtsJunk().
bool isTtsJunk(const std::string& text);

// Split text on sentence-ending punctuation (ASCII .!? and CJK 。！？), keeping
// the delimiter at the end. Mirrors the JS SENTENCE_DELIMITERS lookbehind.
std::vector<std::string> splitSentences(const std::string& s);

} // namespace aoi
