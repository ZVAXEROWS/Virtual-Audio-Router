#pragma once

// =============================================================================
// DeviceManager.h — Virtual Audio Router
// =============================================================================
// Enumerates and monitors WASAPI audio endpoints.
//
// RESPONSIBILITY:
//   - Initialize COM and the MMDevice API
//   - Enumerate all render (playback) and capture devices
//   - Report device properties as DeviceInfo structs
//   - Monitor plug/unplug events via IMMNotificationClient
//   - Notify the engine via EventDispatcher when devices change
//
// INPUTS:
//   - Logger reference (for error reporting)
//   - EventDispatcher reference (for publishing device events)
//
// OUTPUTS:
//   - Result<vector<DeviceInfo>, VarError> from EnumerateDevices()
//   - Events: EvDeviceConnected, EvDeviceDisconnected, EvDeviceStateChanged
//
// DEPENDENCIES:
//   - Windows Multimedia Device API (mmdevapi)
//   - COM (ole32, oleaut32)
//   - Logger, EventDispatcher, core/Types.h
//
// PHASE 1: Stub — COM initialized, returns empty vector.
// PHASE 2: Full WASAPI enumeration implemented.
//
// THREADING:
//   - EnumerateDevices() is called on the control thread.
//   - IMMNotificationClient callbacks arrive on a COM thread; they post
//     to a queue and the device monitor thread processes them.
// =============================================================================

#include <vector>
#include <memory>
#include <string>
#include <functional>
#include "var/Types.h"
#include "var/Result.h"

namespace var {

class Logger;
class EventDispatcher;

class DeviceManager {
public:
    DeviceManager(Logger& logger, EventDispatcher& dispatcher);
    ~DeviceManager();

    // Non-copyable
    DeviceManager(const DeviceManager&)            = delete;
    DeviceManager& operator=(const DeviceManager&) = delete;

    // -------------------------------------------------------------------------
    // Lifecycle
    // -------------------------------------------------------------------------

    /// Initialize COM and the MMDevice enumerator.
    VoidResult Initialize();

    /// Release COM objects and stop monitoring.
    void Shutdown();

    // -------------------------------------------------------------------------
    // Enumeration
    // -------------------------------------------------------------------------

    /// Return all currently active render devices.
    Result<std::vector<DeviceInfo>, VarError> EnumerateDevices(
        DeviceType type = DeviceType::Render);

    /// Return info for a specific device by its endpoint ID string.
    Result<DeviceInfo, VarError> GetDeviceById(const std::string& deviceId);

    /// Return the system default render device.
    Result<DeviceInfo, VarError> GetDefaultDevice(DeviceType type = DeviceType::Render);

    // -------------------------------------------------------------------------
    // Monitoring
    // -------------------------------------------------------------------------

    /// Start monitoring for plug/unplug events.
    VoidResult StartMonitoring();

    /// Stop device monitoring.
    void StopMonitoring();

    bool IsInitialized() const { return m_initialized; }

    /// Set callback invoked when devices change
    void SetDeviceChangeCallback(std::function<void()> callback);

private:
    // Forward-declared; implemented in DeviceManager.cpp using Windows headers
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    Logger&          m_logger;
    EventDispatcher& m_dispatcher;
    bool             m_initialized { false };
};

} // namespace var
