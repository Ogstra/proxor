#pragma once

// macOS Screen Recording permission helpers (phase 53, MAC-QR). The permission is required before
// Qt's screen grab returns anything other than the wallpaper. Objective-C++ implementation, ARC.

namespace ProxorMac {

// CGPreflightScreenCaptureAccess(): never prompts.
bool ScreenCapturePreflight();
// CGRequestScreenCaptureAccess(): may show the macOS prompt (only the first time for a process).
bool ScreenCaptureRequest();
// Opens System Settings > Privacy & Security > Screen & System Audio Recording.
bool OpenScreenRecordingSettings();

} // namespace ProxorMac
