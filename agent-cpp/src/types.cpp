#include "types.hpp"

#include <map>
#include <string>


namespace aoi {

namespace {

const std::map<std::string, MessageType>& typeMap() {
  // Protocol type names may be encrypted by an external build layer for
  // shipped builds; the runtime values are identical either way.
  static const std::map<std::string, MessageType> m = {
      {"greeting", MessageType::Greeting},
      {"user_input", MessageType::UserInput},
      {"assistant_response", MessageType::AssistantResponse},
      {"system_command", MessageType::SystemCommand},
      {"screenshot_request", MessageType::ScreenshotRequest},
      {"screenshot_response", MessageType::ScreenshotResponse},
      {"vr_skill_request", MessageType::VrSkillRequest},
      {"vr_skill_response", MessageType::VrSkillResponse},
      {"error", MessageType::Error},
      {"state_change", MessageType::StateChange},
      {"acknowledge", MessageType::Acknowledge},
      {"heartbeat", MessageType::Heartbeat},
      {"tui_feed", MessageType::TuiFeed},
      {"tui_resize", MessageType::TuiResize},
      {"tui_clear", MessageType::TuiClear},
      {"tui_scroll", MessageType::TuiScroll},
      {"tui_info", MessageType::TuiInfo},
      {"translation", MessageType::Translation},
      {"interpretation_state", MessageType::InterpretationState},
      {"awareness_on", MessageType::AwarenessOn},
      {"awareness_off", MessageType::AwarenessOff},
      {"tts_stop", MessageType::TtsStop},
      {"processing_stage", MessageType::ProcessingStage},
  };
  return m;
}

// Cached decrypted protocol names, so messageTypeToString can keep returning
// const char* (used with printf %s) while the strings stay encrypted in the
// binary. The map is keyed by MessageType int value.
const std::map<int, std::string>& nameCache() {
  static const std::map<int, std::string> c = {
      {static_cast<int>(MessageType::Greeting), "greeting"},
      {static_cast<int>(MessageType::UserInput), "user_input"},
      {static_cast<int>(MessageType::AssistantResponse), "assistant_response"},
      {static_cast<int>(MessageType::SystemCommand), "system_command"},
      {static_cast<int>(MessageType::ScreenshotRequest), "screenshot_request"},
      {static_cast<int>(MessageType::ScreenshotResponse), "screenshot_response"},
      {static_cast<int>(MessageType::VrSkillRequest), "vr_skill_request"},
      {static_cast<int>(MessageType::VrSkillResponse), "vr_skill_response"},
      {static_cast<int>(MessageType::Error), "error"},
      {static_cast<int>(MessageType::StateChange), "state_change"},
      {static_cast<int>(MessageType::Acknowledge), "acknowledge"},
      {static_cast<int>(MessageType::Heartbeat), "heartbeat"},
      {static_cast<int>(MessageType::TuiFeed), "tui_feed"},
      {static_cast<int>(MessageType::TuiResize), "tui_resize"},
      {static_cast<int>(MessageType::TuiClear), "tui_clear"},
      {static_cast<int>(MessageType::TuiScroll), "tui_scroll"},
      {static_cast<int>(MessageType::TuiInfo), "tui_info"},
      {static_cast<int>(MessageType::Translation), "translation"},
      {static_cast<int>(MessageType::InterpretationState), "interpretation_state"},
      {static_cast<int>(MessageType::AwarenessOn), "awareness_on"},
      {static_cast<int>(MessageType::AwarenessOff), "awareness_off"},
      {static_cast<int>(MessageType::TtsStop), "tts_stop"},
      {static_cast<int>(MessageType::ProcessingStage), "processing_stage"},
  };
  return c;
}

} // namespace

MessageType messageTypeFromString(const std::string& s) {
  const auto& m = typeMap();
  auto it = m.find(s);
  return it != m.end() ? it->second : MessageType::Acknowledge;
}

const char* messageTypeToString(MessageType t) {
  const auto& c = nameCache();
  auto it = c.find(static_cast<int>(t));
  if (it != c.end()) return it->second.c_str();
  // Default (unknown type): reuse the same encrypted "acknowledge" value the
  // map holds, so the plaintext never appears in the binary.
  static const std::string fallback = "acknowledge";
  return fallback.c_str();
}

nlohmann::json Message::toJson() const {
  nlohmann::json j;
  j["type"] = messageTypeToString(type);
  j["payload"] = payload.is_null() ? nlohmann::json::object() : payload;
  j["timestamp"] = timestamp;
  j["id"] = id;
  return j;
}

Message Message::fromJson(const nlohmann::json& j) {
  Message m;
  if (j.contains("type") && j["type"].is_string()) {
    m.type = messageTypeFromString(j["type"].get<std::string>());
  }
  if (j.contains("payload")) {
    m.payload = j["payload"];
  }
  if (j.contains("timestamp") && j["timestamp"].is_string()) {
    m.timestamp = j["timestamp"].get<std::string>();
  }
  if (j.contains("id") && j["id"].is_string()) {
    m.id = j["id"].get<std::string>();
  }
  return m;
}

} // namespace aoi
