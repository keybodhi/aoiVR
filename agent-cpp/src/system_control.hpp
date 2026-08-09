#pragma once
#include <string>

namespace aoi {

// Application-level (Windows Volume Mixer slider) volume control via
// ISimpleAudioVolume. Unlike the endpoint master volume, this controls each
// app's own mixer slider, which stays effective even on virtual/streamed
// audio devices (Pico/VR streaming, VB-Cable, etc.) where endpoint volume is
// bypassed.
//
// Every helper takes an optional process filter: empty = every audio session
// on every active render device (behaves like the master slider while moving
// the mixer sliders); a name like "VRChat.exe" (case-insensitive, ".exe"
// optional) = only that application's session.

// Set session volume (0-100) for `process` (or all sessions if empty).
// Returns "" on success.
std::string setSessionVolumes(const std::string& process, int percent);

// Mute / unmute sessions for `process` (or all sessions if empty).
std::string setSessionsMute(const std::string& process, bool mute);

// Describe all render devices and their audio sessions
// ("device: name\n  proc.exe vol=xx% mute=no ..."). Returns "" on success.
std::string getSessionVolumes(std::string& outDesc);

} // namespace aoi
