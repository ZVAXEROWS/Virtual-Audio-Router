#pragma once

// =============================================================================
// OutputDevice.h — Virtual Audio Router
// =============================================================================
// Concrete WASAPI implementation of IDevice for a render endpoint.
//
// RESPONSIBILITY:
//   Open a specific WASAPI endpoint in shared mode, negotiate the audio format,
//   and provide a Write() method that pushes PCM frames to the device buffer.
//
// INPUTS:
//   - DeviceInfo identifying the target endpoint
//   - AudioFormat describing the data format the engine will supply
//
// OUTPUTS:
//   - Consumes PCM audio frames via Write()
//   - Reports playback position and latency
//
// DEPENDENCIES:
//   - IDevice interface
//   - Windows WASAPI (IAudioClient, IAudioRenderClient)
//   - Logger
//
// THREADING:
//   - Initialize/Start/Stop/Shutdown are called on the control thread.
//   - Write() is called ONLY on the dedicated output thread for this device.
//   - Device is opened in SHARED mode in Phase 3; EXCLUSIVE mode option in Phase 5.
//
// FUTURE:
//   - Exclusive mode for ultra-low latency (Phase 5)
//   - Hardware-accelerated resampling (Phase 4)
// =============================================================================

#include "var/IDevice.h"
#include "var/Types.h"
#include "var/Result.h"
#include <memory>

namespace var {

class Logger;

class OutputDevice final : public IDevice {
public:
    OutputDevice(DeviceInfo info, Logger& logger);
    ~OutputDevice() override;

    // Non-copyable
    OutputDevice(const OutputDevice&)            = delete;
    OutputDevice& operator=(const OutputDevice&) = delete;

    // -------------------------------------------------------------------------
    // IDevice implementation
    // -------------------------------------------------------------------------

    VoidResult Initialize(const AudioFormat& requestedFormat) override;
    VoidResult Start() override;
    VoidResult Stop() override;
    void       Shutdown() override;
    VoidResult Write(const float* data, uint32_t frameCount) override;

    const DeviceInfo&  GetDeviceInfo()   const override { return m_info; }
    const AudioFormat& GetActualFormat() const override { return m_actualFormat; }
    uint64_t           GetPosition()     const override { return m_position.load(); }
    bool               IsRunning()       const override { return m_running.load(); }
    double             GetLatencyMs()    const override { return m_latencyMs; }

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;   ///< COM objects hidden from header

    DeviceInfo            m_info;
    AudioFormat           m_actualFormat {};
    std::atomic<uint64_t> m_position    { 0 };
    std::atomic<bool>     m_running     { false };
    double                m_latencyMs   { 0.0 };
    Logger&               m_logger;
};

} // namespace var
