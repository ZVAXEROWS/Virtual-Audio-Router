// =============================================================================
// OutputDevice.cpp — Virtual Audio Router
// =============================================================================
// Phase 1: Stub implementation.
// Phase 3: Full WASAPI IAudioClient / IAudioRenderClient implemented here.
// =============================================================================

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>

#include "var/OutputDevice.h"
#include "var/Logger.h"

namespace var {

struct OutputDevice::Impl {
    // Phase 3: IAudioClient*, IAudioRenderClient* go here
};

OutputDevice::OutputDevice(DeviceInfo info, Logger& logger)
    : m_impl(std::make_unique<Impl>())
    , m_info(std::move(info))
    , m_logger(logger)
{}

OutputDevice::~OutputDevice() {
    if (m_running.load()) {
        Stop();
    }
    Shutdown();
}

VoidResult OutputDevice::Initialize(const AudioFormat& requestedFormat) {
    m_logger.Info("OutputDevice: Initialize called for '" + m_info.name +
                  "' (Phase 1 stub).");
    m_actualFormat = requestedFormat;
    return VoidResult::ok();
}

VoidResult OutputDevice::Start() {
    m_logger.Info("OutputDevice: Start called for '" + m_info.name +
                  "' (Phase 1 stub).");
    m_running.store(true);
    return VoidResult::ok();
}

VoidResult OutputDevice::Stop() {
    m_logger.Info("OutputDevice: Stop called for '" + m_info.name +
                  "' (Phase 1 stub).");
    m_running.store(false);
    return VoidResult::ok();
}

void OutputDevice::Shutdown() {
    m_logger.Info("OutputDevice: Shutdown called for '" + m_info.name +
                  "' (Phase 1 stub).");
}

VoidResult OutputDevice::Write(const float* /*data*/, uint32_t frameCount) {
    // Phase 1: do nothing, just advance position counter
    m_position.fetch_add(frameCount);
    return VoidResult::ok();
}

} // namespace var
