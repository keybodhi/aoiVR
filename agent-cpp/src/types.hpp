#pragma once
#include <string>
#include <unordered_map>
#include <nlohmann/json.hpp>

namespace aoi {

// NOTE: the agent now uses an in-process transport (C ABI SendJson / callback);
// no named pipe is used, so no pipe constants are defined here.

enum class MessageType {
  Greeting = 0,
  UserInput,
  AssistantResponse,
  SystemCommand,
  ScreenshotRequest,
  ScreenshotResponse,
  VrSkillRequest,
  VrSkillResponse,
  Error,
  StateChange,
  Acknowledge,
  Heartbeat,
  TuiFeed,
  TuiResize,
  TuiClear,
  TuiScroll,
  TuiInfo,
  Translation,
  InterpretationState,
  AwarenessOn,
  AwarenessOff,
  TtsStop,
  ProcessingStage,
};

// Convert a wire string to a MessageType; returns std::nullopt on unknown.
MessageType messageTypeFromString(const std::string& s);
const char* messageTypeToString(MessageType t);

struct Message {
  MessageType type = MessageType::Acknowledge;
  nlohmann::json payload;
  std::string timestamp;
  std::string id;

  nlohmann::json toJson() const;
  static Message fromJson(const nlohmann::json& j);
};

} // namespace aoi
