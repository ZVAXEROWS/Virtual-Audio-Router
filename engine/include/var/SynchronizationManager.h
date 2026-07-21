#pragma once

// =============================================================================
// SynchronizationManager.h — Virtual Audio Router
// =============================================================================
// Manages clock synchronization and drift correction across output devices.
//
// RESPONSIBILITY:
//   Even after latency compensation, device clocks drift over time. A USB
//   speaker may run at 48001 Hz (1 Hz fast), a Bluetooth device at 47999 Hz
//   (1 Hz slow). After 1 hour, these are ~3.6 seconds apart — audible as
//   one device cutting out before the other.
//
//   SynchronizationManager:
//     1. Tracks each device's audio clock (reported by WASAPI GetPosition).
//     2. Calculates drift relative to a master clock (wall clock or capture device).
//     3. Signals the Resampler to slightly speed up or slow down each device's
//        stream to maintain alignment.
//
// PHASE 1: Stub header. Data structures defined.
// PHASE 5: Full PLL (Phase-Locked Loop) based drift correction.
//
// REFERENCE:
//   The algorithm used will follow the approach in JACK Audio Connection Kit's
//   drift correction: monitor clock delta over a rolling window, apply a
//   fractional resampling ratio correction.
// =============================================================================

#include <string>
#include <unordered_map>
#include <chrono>
#include "var/Types.h"
#include "var/Result.h"

namespace var {

class Logger;

struct ClockState {
    std::string  deviceId;
    uint64_t     lastPosition    { 0 };  ///< Last reported position in frames
    double       driftPpm        { 0.0 }; ///< Parts-per-million drift from master
    double       correctionRatio { 1.0 }; ///< Resampling ratio correction (1.0 = no correction)
    std::chrono::steady_clock::time_point lastUpdate;
};

class SynchronizationManager {
public:
    explicit SynchronizationManager(Logger& logger);
    ~SynchronizationManager();

    SynchronizationManager(const SynchronizationManager&)            = delete;
    SynchronizationManager& operator=(const SynchronizationManager&) = delete;

    // -------------------------------------------------------------------------
    // Lifecycle
    // -------------------------------------------------------------------------

    VoidResult Initialize();
    void       Shutdown();

    // -------------------------------------------------------------------------
    // Clock tracking
    // -------------------------------------------------------------------------

    /// Register a device to be tracked.
    void RegisterDevice(const std::string& deviceId, uint32_t sampleRate);

    /// Update a device's reported position (called each buffer period).
    void UpdatePosition(const std::string& deviceId, uint64_t positionFrames);

    /// Get the recommended resampling ratio correction for a device.
    /// Values > 1.0 mean the device is running slow; < 1.0 means fast.
    double GetCorrectionRatio(const std::string& deviceId) const;

    /// Remove a device from tracking.
    void UnregisterDevice(const std::string& deviceId);

    // -------------------------------------------------------------------------
    // Master clock
    // -------------------------------------------------------------------------

    /// Reset synchronization state (e.g. after routing restarts).
    void Reset();

    const std::unordered_map<std::string, ClockState>& GetClockStates() const {
        return m_clocks;
    }

private:
    std::unordered_map<std::string, ClockState> m_clocks;
    std::chrono::steady_clock::time_point       m_epochTime;
    Logger&                                     m_logger;
    bool                                        m_initialized { false };
};

} // namespace var
