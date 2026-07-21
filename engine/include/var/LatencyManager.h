#pragma once

// =============================================================================
// LatencyManager.h — Virtual Audio Router
// =============================================================================
// Measures and compensates for per-device latency differences.
//
// RESPONSIBILITY:
//   When routing audio to multiple devices simultaneously, each device has a
//   different hardware latency (USB: ~3ms, HDMI: ~10ms, Bluetooth: 30-200ms).
//   Without compensation, all devices play the same buffer at the same wall-
//   clock time, but the user hears them offset because of downstream hardware delay.
//
//   LatencyManager:
//     1. Measures the reported latency of each output device.
//     2. Calculates the maximum latency across all active outputs.
//     3. Instructs lower-latency devices to delay their playback by
//        (maxLatency - deviceLatency) so all devices are perceptually aligned.
//
// PHASE 1: Stub header. Data types defined.
// PHASE 5: Full implementation with drift correction.
//
// NOTE ON BLUETOOTH:
//   Bluetooth A2DP latency is device-specific and not reliably reported by
//   the Windows audio stack. Phase 5 implements empirical latency measurement.
// =============================================================================

#include <vector>
#include <string>
#include <unordered_map>
#include "var/Types.h"
#include "var/Result.h"

namespace var {

class Logger;

struct DeviceLatencyProfile {
    std::string deviceId;
    double      reportedLatencyMs { 0.0 };   ///< From WASAPI GetStreamLatency
    double      measuredLatencyMs { 0.0 };   ///< Empirically measured (Phase 5)
    double      compensationMs    { 0.0 };   ///< Delay to add to this device
};

class LatencyManager {
public:
    explicit LatencyManager(Logger& logger);
    ~LatencyManager();

    LatencyManager(const LatencyManager&)            = delete;
    LatencyManager& operator=(const LatencyManager&) = delete;

    // -------------------------------------------------------------------------
    // Setup
    // -------------------------------------------------------------------------

    VoidResult Initialize();
    void       Shutdown();

    /// Register a device's reported latency.
    void RegisterDevice(const std::string& deviceId, double reportedLatencyMs);

    /// Remove a device from latency tracking.
    void UnregisterDevice(const std::string& deviceId);

    // -------------------------------------------------------------------------
    // Compensation
    // -------------------------------------------------------------------------

    /// Recalculate compensation values for all registered devices.
    /// Call this after adding/removing devices.
    void RecalculateCompensation();

    /// Get the compensation delay (in ms) for a specific device.
    double GetCompensationMs(const std::string& deviceId) const;

    /// Get compensation in frames for a given sample rate.
    uint32_t GetCompensationFrames(const std::string& deviceId, uint32_t sampleRate) const;

    const std::vector<DeviceLatencyProfile>& GetProfiles() const { return m_profiles; }

private:
    std::vector<DeviceLatencyProfile>              m_profiles;
    std::unordered_map<std::string, size_t>        m_idToIndex;
    Logger&                                        m_logger;
    bool                                           m_initialized { false };
};

} // namespace var
