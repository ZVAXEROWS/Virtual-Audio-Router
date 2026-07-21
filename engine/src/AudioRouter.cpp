// =============================================================================
// AudioRouter.cpp — Virtual Audio Router
// =============================================================================
// Phase 1: Stub. Phase 4: Multi-device fan-out implemented.
// =============================================================================

#include "var/AudioRouter.h"
#include "var/IDevice.h"
#include "var/Logger.h"
#include "var/BufferManager.h"
#include "var/LatencyManager.h"
#include <mutex>
#include <vector>

namespace var {

struct AudioRouter::Impl {
    std::vector<IDevice*>   outputs;
    mutable std::mutex      outputsMutex;
};

AudioRouter::AudioRouter(Logger& logger, BufferManager& buffer,
                         LatencyManager& latency)
    : m_impl(std::make_unique<Impl>())
    , m_logger(logger)
    , m_buffer(buffer)
    , m_latency(latency)
{}

AudioRouter::~AudioRouter() {
    Shutdown();
}

VoidResult AudioRouter::Initialize() {
    m_logger.Info("AudioRouter: Initialized (Phase 1 stub).");
    return VoidResult::ok();
}

void AudioRouter::Shutdown() {
    m_routing.store(false);
    std::lock_guard lock(m_impl->outputsMutex);
    m_impl->outputs.clear();
    m_logger.Info("AudioRouter: Shutdown complete.");
}

VoidResult AudioRouter::ConfigureOutputs(std::vector<IDevice*> devices) {
    std::lock_guard lock(m_impl->outputsMutex);
    m_impl->outputs = std::move(devices);
    m_logger.Info("AudioRouter: Configured with " +
                  std::to_string(m_impl->outputs.size()) + " output(s).");
    return VoidResult::ok();
}

void AudioRouter::RemoveDevice(const std::string& deviceId) {
    std::lock_guard lock(m_impl->outputsMutex);
    auto& v = m_impl->outputs;
    v.erase(std::remove_if(v.begin(), v.end(),
        [&deviceId](IDevice* d) {
            return d->GetDeviceInfo().id == deviceId;
        }), v.end());
    m_logger.Info("AudioRouter: Removed device " + deviceId);
}

VoidResult AudioRouter::RouteBuffer(uint32_t /*frameCount*/) {
    // Phase 1: no-op. Phase 4: read from BufferManager, write to each IDevice.
    return VoidResult::ok();
}

uint32_t AudioRouter::OutputDeviceCount() const {
    std::lock_guard lock(m_impl->outputsMutex);
    return static_cast<uint32_t>(m_impl->outputs.size());
}

} // namespace var
