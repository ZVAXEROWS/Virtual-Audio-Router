#pragma once

// =============================================================================
// Types.h — Virtual Audio Router
// =============================================================================
// All data-transfer types shared across C++ modules AND exposed to Python via
// pybind11. Nothing in this header may depend on engine internals.
//
// DESIGN RULES:
//   1. These are plain-old-data (POD) / value-semantic types — no behaviour.
//   2. All strings use std::string (not wstring); pybind11 maps to Python str.
//   3. Every field has a default so zero-initialisation is safe.
//   4. Prefer enums over raw integers for state/type fields.
// =============================================================================

#include <string>
#include <vector>
#include <cstdint>
#include <optional>

namespace var {

// ---------------------------------------------------------------------------
// DeviceState — mirrors WASAPI DEVICE_STATE_* flags
// ---------------------------------------------------------------------------

enum class DeviceState : uint32_t {
    Active      = 0x00000001,  ///< Device present and enabled
    Disabled    = 0x00000002,  ///< Disabled in Control Panel
    NotPresent  = 0x00000004,  ///< Hardware removed
    Unplugged   = 0x00000008,  ///< Jack unplugged (jack-presence detection)
    Unknown     = 0xFFFFFFFF,
};

// ---------------------------------------------------------------------------
// DeviceType — data flow direction
// ---------------------------------------------------------------------------

enum class DeviceType : uint8_t {
    Render  = 0,  ///< Playback (speakers, headphones)
    Capture = 1,  ///< Recording (microphone, loopback)
};

// ---------------------------------------------------------------------------
// AudioFormat — the mix format negotiated with a WASAPI endpoint
// ---------------------------------------------------------------------------

struct AudioFormat {
    uint32_t sampleRate      { 48000 };  ///< Samples per second (e.g. 44100, 48000)
    uint16_t channels        { 2     };  ///< Number of channels
    uint16_t bitsPerSample   { 32    };  ///< Bits per sample (16, 24, 32)
    uint32_t bufferFrames    { 480   };  ///< Buffer size in frames
    bool     isFloat         { true  };  ///< True = IEEE float; False = PCM int

    /// Bytes per frame
    uint32_t bytesPerFrame() const {
        return static_cast<uint32_t>(channels) * (bitsPerSample / 8u);
    }

    /// Total bytes in one buffer period
    uint32_t bytesPerBuffer() const {
        return bufferFrames * bytesPerFrame();
    }

    /// Buffer duration in milliseconds
    double bufferMs() const {
        return (sampleRate > 0)
            ? (static_cast<double>(bufferFrames) / sampleRate) * 1000.0
            : 0.0;
    }
};

// ---------------------------------------------------------------------------
// DeviceInfo — snapshot of a WASAPI endpoint's properties
// ---------------------------------------------------------------------------

struct DeviceInfo {
    std::string   id          {};         ///< Unique endpoint ID (from IMMDevice)
    std::string   name        {};         ///< Human-readable name
    std::string   description {};         ///< Driver description
    DeviceState   state       { DeviceState::Unknown };
    DeviceType    type        { DeviceType::Render   };
    bool          isDefault   { false };  ///< Is this the system default?
    AudioFormat   nativeFormat{};         ///< Format reported by the driver
    double        latencyMs   { 0.0  };   ///< Reported minimum latency
};

// ---------------------------------------------------------------------------
// RouterConfig — runtime routing configuration
// ---------------------------------------------------------------------------

struct RouterConfig {
    std::string              inputDeviceId  {};   ///< Source device ("" = loopback)
    std::vector<std::string> outputDeviceIds{};   ///< Target device IDs
    uint32_t                 bufferSizeMs   { 20 }; ///< Desired buffer in ms
    bool                     enableResampling{ true };
    bool                     enableDriftCorrection{ true };
};

// ---------------------------------------------------------------------------
// EngineStatus — state machine for AudioEngine lifecycle
// ---------------------------------------------------------------------------

enum class EngineStatus : uint8_t {
    Uninitialized = 0,
    Initializing  = 1,
    Ready         = 2,  ///< Initialized, not routing
    Routing       = 3,  ///< Actively routing audio
    Stopping      = 4,
    Error         = 5,
    ShuttingDown  = 6,
};

// ---------------------------------------------------------------------------
// LogEntry — a single log message readable from Python
// ---------------------------------------------------------------------------

enum class LogLevel : uint8_t {
    Debug   = 0,
    Info    = 1,
    Warning = 2,
    Error   = 3,
    Fatal   = 4,
};

struct LogEntry {
    LogLevel    level     { LogLevel::Info };
    std::string message   {};
    std::string source    {};     ///< File:line or module name
    int64_t     timestampMs{ 0 }; ///< Milliseconds since engine init
};

// ---------------------------------------------------------------------------
// Profile — named routing preset (saved/loaded by SettingsManager)
// ---------------------------------------------------------------------------

struct Profile {
    std::string  name     { "Default" };
    RouterConfig config   {};
    bool         isActive { false };
};

} // namespace var
