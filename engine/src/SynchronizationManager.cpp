// =============================================================================
// SynchronizationManager.cpp — Virtual Audio Router
// =============================================================================
// Phase 1: Stub. Phase 5: PLL-based clock drift correction.
// =============================================================================

#include "var/SynchronizationManager.h"
#include "var/Logger.h"

namespace var {

SynchronizationManager::SynchronizationManager(Logger& logger)
    : m_logger(logger)
{}

SynchronizationManager::~SynchronizationManager() { Shutdown(); }

VoidResult SynchronizationManager::Initialize() {
    m_epochTime   = std::chrono::steady_clock::now();
    m_initialized = true;
    m_logger.Info("SynchronizationManager: Initialized (Phase 1 stub).");
    return VoidResult::ok();
}

void SynchronizationManager::Shutdown() {
    m_clocks.clear();
    m_initialized = false;
}

void SynchronizationManager::RegisterDevice(const std::string& deviceId,
                                             uint32_t /*sampleRate*/) {
    ClockState cs;
    cs.deviceId    = deviceId;
    cs.lastUpdate  = std::chrono::steady_clock::now();
    m_clocks[deviceId] = cs;
    m_logger.Info("SynchronizationManager: Registered device " + deviceId);
}

void SynchronizationManager::UpdatePosition(const std::string& deviceId,
                                             uint64_t positionFrames) {
    auto it = m_clocks.find(deviceId);
    if (it == m_clocks.end()) return;
    it->second.lastPosition = positionFrames;
    it->second.lastUpdate   = std::chrono::steady_clock::now();
    // Phase 5: compute drift against master clock here
}

double SynchronizationManager::GetCorrectionRatio(const std::string& deviceId) const {
    auto it = m_clocks.find(deviceId);
    if (it == m_clocks.end()) return 1.0;
    return it->second.correctionRatio; // Phase 5: computed by PLL
}

void SynchronizationManager::UnregisterDevice(const std::string& deviceId) {
    m_clocks.erase(deviceId);
}

void SynchronizationManager::Reset() {
    for (auto& [id, cs] : m_clocks) {
        cs.lastPosition    = 0;
        cs.driftPpm        = 0.0;
        cs.correctionRatio = 1.0;
    }
    m_epochTime = std::chrono::steady_clock::now();
}

} // namespace var
