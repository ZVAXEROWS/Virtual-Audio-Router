#pragma once

// =============================================================================
// Constants.h — Virtual Audio Router
// =============================================================================
// Compile-time constants shared across all C++ modules. Values chosen based on
// professional audio engineering best practices and WASAPI recommendations.
// =============================================================================

#include <cstdint>
#include <string_view>

namespace var::constants {

// ---------------------------------------------------------------------------
// Version
// ---------------------------------------------------------------------------

constexpr std::string_view kVersionString    = "0.1.0";
constexpr uint32_t         kVersionMajor     = 0;
constexpr uint32_t         kVersionMinor     = 1;
constexpr uint32_t         kVersionPatch     = 0;

// ---------------------------------------------------------------------------
// Audio
// ---------------------------------------------------------------------------

/// Maximum number of simultaneous output devices supported
constexpr uint32_t kMaxOutputDevices = 16;

/// Default buffer size in milliseconds.
/// 20ms is a balanced choice: low enough to feel responsive,
/// large enough that Bluetooth devices (30-50ms native latency)
/// don't underrun constantly.
constexpr uint32_t kDefaultBufferMs = 20;

/// Minimum buffer size (aggressive low-latency mode)
constexpr uint32_t kMinBufferMs = 5;

/// Maximum buffer size (high-latency safe mode for Bluetooth)
constexpr uint32_t kMaxBufferMs = 200;

/// Preferred sample rate for internal mixing bus
constexpr uint32_t kDefaultSampleRate = 48000;

/// Internal mixing channel count
constexpr uint32_t kDefaultChannels = 2;

/// Internal mixing bit depth (IEEE float)
constexpr uint32_t kDefaultBitsPerSample = 32;

// ---------------------------------------------------------------------------
// Threading
// ---------------------------------------------------------------------------

/// Number of worker threads in the general-purpose thread pool.
/// Audio threads are separate and created explicitly.
constexpr uint32_t kThreadPoolSize = 4;

/// Size of the logger's async message queue (ring buffer capacity)
constexpr uint32_t kLoggerQueueSize = 4096;

/// Maximum log entries kept in memory for Python polling
constexpr uint32_t kMaxInMemoryLogEntries = 1000;

// ---------------------------------------------------------------------------
// Device monitoring
// ---------------------------------------------------------------------------

/// How often (ms) the device monitor thread polls for device changes.
/// WASAPI also provides IMMNotificationClient callbacks; polling is a fallback.
constexpr uint32_t kDeviceMonitorIntervalMs = 2000;

/// Time to wait before attempting to reconnect a disconnected device
constexpr uint32_t kReconnectDelayMs = 3000;

/// Maximum reconnect attempts before giving up
constexpr uint32_t kMaxReconnectAttempts = 10;

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

constexpr std::string_view kDefaultConfigFilename = "settings.json";
constexpr std::string_view kDefaultProfileName    = "Default";
constexpr std::string_view kLogFilename           = "var_engine.log";
constexpr std::string_view kAppName               = "Virtual Audio Router";
constexpr std::string_view kAppId                 = "com.var.VirtualAudioRouter";

// ---------------------------------------------------------------------------
// Timing
// ---------------------------------------------------------------------------

/// WASAPI reference time units per second (100-nanosecond intervals)
constexpr int64_t kReftimesPerSec = 10'000'000LL;

/// Convert milliseconds to WASAPI REFERENCE_TIME units
constexpr int64_t msToReferenceTime(uint32_t ms) {
    return static_cast<int64_t>(ms) * (kReftimesPerSec / 1000);
}

} // namespace var::constants
