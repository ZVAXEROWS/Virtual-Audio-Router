#pragma once

// =============================================================================
// AudioEngine.h — Virtual Audio Router
// =============================================================================
// Top-level orchestrator and the ONLY class exposed to Python via pybind11.
//
// RESPONSIBILITY:
//   The Façade over all engine subsystems. Python calls only methods on this
//   class; it delegates to DeviceManager, AudioRouter, BufferManager, etc.
//
//   Owns the lifecycle of:
//     - Logger             (created first, destroyed last)
//     - EventDispatcher    (pub/sub bus)
//     - ThreadPool         (background workers)
//     - DeviceManager      (WASAPI enumeration)
//     - BufferManager      (shared audio ring buffer)
//     - LatencyManager     (per-device latency)
//     - SynchronizationManager (clock drift)
//     - AudioRouter        (fan-out routing)
//     - OutputDevice(s)    (one per active output)
//
// PYTHON API SURFACE (exposed via pybind11):
//   engine = var_engine.AudioEngine()
//   engine.initialize(log_dir)
//   devices = engine.get_devices()        -> list[dict]
//   engine.start_routing(config_dict)
//   engine.stop_routing()
//   status = engine.get_status()          -> str
//   logs = engine.get_recent_logs(n)      -> list[dict]
//   engine.shutdown()
//
// WHY FAÇADE:
//   Minimizes pybind11 surface area. Python never holds references into
//   DeviceManager or BufferManager — only AudioEngine. If the engine is
//   replaced with a different backend (socket-based, in-process driver, etc.),
//   only var_bindings.cpp changes.
//
// THREADING:
//   - All public methods are safe to call from the Python thread.
//   - Methods post work to background threads; they never block the UI.
// =============================================================================

#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include "var/Types.h"
#include "var/Result.h"

namespace var {

class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    // Non-copyable, non-movable
    AudioEngine(const AudioEngine&)            = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;
    AudioEngine(AudioEngine&&)                 = delete;
    AudioEngine& operator=(AudioEngine&&)      = delete;

    // =========================================================================
    // Lifecycle (called from Python)
    // =========================================================================

    /// Initialize all subsystems. Must be called before any other method.
    /// @param logDirectory  Path where var_engine.log will be written.
    VoidResult Initialize(const std::string& logDirectory = ".");

    /// Cleanly shut down all subsystems and release resources.
    void Shutdown();

    // =========================================================================
    // Device management (called from Python)
    // =========================================================================

    /// Enumerate all available render devices.
    /// Returns a vector of DeviceInfo (serialised to Python dicts by pybind11).
    Result<std::vector<DeviceInfo>, VarError> GetDevices();

    /// Return info for the system default render device.
    Result<DeviceInfo, VarError> GetDefaultDevice();

    // =========================================================================
    // Routing control (called from Python)
    // =========================================================================

    /// Apply a RouterConfig and begin routing audio.
    VoidResult StartRouting(const RouterConfig& config);

    /// Stop all active routing.
    VoidResult StopRouting();

    // =========================================================================
    // Status & diagnostics (called from Python)
    // =========================================================================

    EngineStatus GetStatus() const { return m_status.load(); }

    /// Get recent log entries for display in the Python log panel.
    std::vector<LogEntry> GetRecentLogs(uint32_t maxEntries = 100) const;

    /// Get the currently active configuration.
    RouterConfig GetCurrentConfig() const;

    // =========================================================================
    // Internal helpers (not exposed to Python)
    // =========================================================================
private:
    void SetStatus(EngineStatus status);

    struct Impl;
    std::unique_ptr<Impl> m_impl;         ///< All subsystems stored here
    std::atomic<EngineStatus> m_status   { EngineStatus::Uninitialized };
};

} // namespace var
