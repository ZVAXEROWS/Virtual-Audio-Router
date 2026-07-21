// =============================================================================
// BufferManager.cpp — Virtual Audio Router
// =============================================================================
// Phase 1: Stub. Phase 4: Full multi-reader ring buffer.
// =============================================================================

#include "var/BufferManager.h"
#include "var/Logger.h"
#include <vector>
#include <mutex>
#include <unordered_map>
#include <atomic>

namespace var {

struct BufferManager::Impl {
    std::vector<float>                        buffer;
    std::unordered_map<uint32_t, uint32_t>    readerPositions;
    std::atomic<uint32_t>                     writePosition { 0 };
    std::atomic<uint32_t>                     nextReaderId  { 0 };
    mutable std::mutex                        mutex;
};

BufferManager::BufferManager(Logger& logger)
    : m_impl(std::make_unique<Impl>())
    , m_logger(logger)
{}

BufferManager::~BufferManager() {
    Shutdown();
}

VoidResult BufferManager::Initialize(const AudioFormat& format, uint32_t capacityMs) {
    m_format = format;
    m_capacityFrames = (format.sampleRate * capacityMs) / 1000;
    m_logger.Info("BufferManager: Initialized. Capacity=" +
                  std::to_string(m_capacityFrames) + " frames (Phase 1 stub).");
    m_initialized = true;
    return VoidResult::ok();
}

void BufferManager::Shutdown() {
    m_initialized = false;
    m_logger.Info("BufferManager: Shutdown.");
}

VoidResult BufferManager::Write(const float* /*data*/, uint32_t /*frameCount*/) {
    return VoidResult::ok(); // Phase 4
}

uint32_t BufferManager::RegisterReader() {
    std::lock_guard lock(m_impl->mutex);
    uint32_t id = m_impl->nextReaderId.fetch_add(1);
    m_impl->readerPositions[id] = m_impl->writePosition.load();
    return id;
}

VoidResult BufferManager::Read(uint32_t /*readerId*/, float* outData,
                                uint32_t frameCount) {
    // Phase 1: Return silence
    std::fill(outData, outData + frameCount * m_format.channels, 0.0f);
    return VoidResult::ok();
}

uint32_t BufferManager::Available(uint32_t /*readerId*/) const {
    return 0; // Phase 4
}

} // namespace var
