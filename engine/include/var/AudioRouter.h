#pragma once

// =============================================================================
// AudioRouter.h — Virtual Audio Router
// =============================================================================
// Routes audio from one source to multiple IDevice outputs.
//
// RESPONSIBILITY:
//   The core routing engine. Receives a buffer of PCM audio from the capture
//   side and fans it out to every registered output device — applying resampling
//   and latency compensation as needed.
//
// INPUTS:
//   - A vector of IDevice* (output sinks)
//   - PCM audio frames from the capture thread (via BufferManager)
//   - Resampler and LatencyManager for per-device adjustments
//
// OUTPUTS:
//   - Calls IDevice::Write() on each output device
//
// DESIGN:
//   AudioRouter does NOT own IDevice objects — it holds non-owning raw pointers.
//   Ownership stays with AudioEngine (which owns the OutputDevice instances).
//   This prevents lifetime issues when devices are hot-unplugged.
//
// THREADING:
//   - ConfigureOutputs() is called on the control thread.
//   - RouteBuffer() is called on the audio routing thread (NOT the audio thread).
//
// PHASE 1: Stub — no routing logic.
// PHASE 4: Multi-device fan-out implemented.
// =============================================================================

#include <vector>
#include <memory>
#include "var/Types.h"
#include "var/Result.h"

namespace var {

class IDevice;
class Logger;
class BufferManager;
class Resampler;
class LatencyManager;

class AudioRouter {
public:
    AudioRouter(Logger& logger, BufferManager& buffer,
                LatencyManager& latency);
    ~AudioRouter();

    AudioRouter(const AudioRouter&)            = delete;
    AudioRouter& operator=(const AudioRouter&) = delete;

    // -------------------------------------------------------------------------
    // Lifecycle
    // -------------------------------------------------------------------------

    VoidResult Initialize();
    void       Shutdown();

    // -------------------------------------------------------------------------
    // Device management
    // -------------------------------------------------------------------------

    /// Replace the set of active output devices.
    /// Called on the control thread; takes effect on the next RouteBuffer call.
    VoidResult ConfigureOutputs(std::vector<IDevice*> devices);

    /// Remove a device mid-stream (e.g. hot-unplug).
    void RemoveDevice(const std::string& deviceId);

    // -------------------------------------------------------------------------
    // Routing
    // -------------------------------------------------------------------------

    /// Distribute frameCount frames from the BufferManager to all output devices.
    /// Called repeatedly by the routing thread.
    VoidResult RouteBuffer(uint32_t frameCount);

    uint32_t OutputDeviceCount() const;
    bool     IsRouting()         const { return m_routing.load(); }

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    Logger&         m_logger;
    BufferManager&  m_buffer;
    LatencyManager& m_latency;
    std::atomic<bool> m_routing { false };
};

} // namespace var
