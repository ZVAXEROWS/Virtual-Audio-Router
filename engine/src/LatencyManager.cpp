// =============================================================================
// LatencyManager.cpp — Virtual Audio Router
// =============================================================================
// Phase 1: Stub. Phase 5: PLL-based drift and latency correction.
// =============================================================================

#include "var/LatencyManager.h"
#include "var/Logger.h"
#include <algorithm>
#include <cmath>

namespace var {

LatencyManager::LatencyManager(Logger& logger)
    : m_logger(logger)
{}

LatencyManager::~LatencyManager() { Shutdown(); }

VoidResult LatencyManager::Initialize() {
    m_initialized = true;
    m_logger.Info("LatencyManager: Initialized (Phase 1 stub).");
    return VoidResult::ok();
}

void LatencyManager::Shutdown() {
    m_profiles.clear();
    m_idToIndex.clear();
    m_initialized = false;
}

void LatencyManager::RegisterDevice(const std::string& deviceId, double reportedLatencyMs) {
    if (m_idToIndex.count(deviceId)) return;
    DeviceLatencyProfile p;
    p.deviceId          = deviceId;
    p.reportedLatencyMs = reportedLatencyMs;
    m_idToIndex[deviceId] = m_profiles.size();
    m_profiles.push_back(std::move(p));
    m_logger.Info("LatencyManager: Registered device " + deviceId +
                  " latency=" + std::to_string(reportedLatencyMs) + "ms");
}

void LatencyManager::UnregisterDevice(const std::string& deviceId) {
    auto it = m_idToIndex.find(deviceId);
    if (it == m_idToIndex.end()) return;
    m_profiles.erase(m_profiles.begin() + it->second);
    m_idToIndex.erase(it);
    // Rebuild index map
    for (size_t i = 0; i < m_profiles.size(); ++i)
        m_idToIndex[m_profiles[i].deviceId] = i;
}

void LatencyManager::RecalculateCompensation() {
    if (m_profiles.empty()) return;
    double maxLatency = 0.0;
    for (const auto& p : m_profiles)
        maxLatency = std::max(maxLatency, p.reportedLatencyMs);
    for (auto& p : m_profiles)
        p.compensationMs = maxLatency - p.reportedLatencyMs;
    m_logger.Info("LatencyManager: RecalculateCompensation. Max latency=" +
                  std::to_string(maxLatency) + "ms");
}

double LatencyManager::GetCompensationMs(const std::string& deviceId) const {
    auto it = m_idToIndex.find(deviceId);
    if (it == m_idToIndex.end()) return 0.0;
    return m_profiles[it->second].compensationMs;
}

uint32_t LatencyManager::GetCompensationFrames(const std::string& deviceId,
                                                uint32_t sampleRate) const {
    double ms = GetCompensationMs(deviceId);
    return static_cast<uint32_t>(std::round(ms * sampleRate / 1000.0));
}

} // namespace var
