#pragma once

// =============================================================================
// IDevice.h — Virtual Audio Router
// =============================================================================
// Pure abstract interface for any audio output sink.
//
// RESPONSIBILITY:
//   Defines the contract that ANY output sink must satisfy — whether it is:
//     - A real WASAPI endpoint (OutputDevice)
//     - A future virtual driver endpoint (driver/)
//     - A file writer (for testing/recording)
//     - A network audio sink (future)
//
// WHY AN INTERFACE HERE:
//   AudioRouter holds vector<IDevice*>. This means the router doesn't know or
//   care what kind of sink it is sending audio to. Adding the WDK virtual
//   driver in Phase 8 requires zero changes to AudioRouter — the driver just
//   implements IDevice.
//
// THREADING CONTRACT:
//   - Initialize() is called on the control thread.
//   - Write() is called on a dedicated high-priority audio output thread.
//   - Shutdown() is called on the control thread AFTER stopping the audio thread.
//   - Implementations MUST be thread-safe between control and audio threads.
// =============================================================================

#include <cstdint>
#include <string>
#include "var/Types.h"
#include "var/Result.h"

namespace var {

class IDevice {
public:
    virtual ~IDevice() = default;

    // -------------------------------------------------------------------------
    // Lifecycle
    // -------------------------------------------------------------------------

    /// Open the device and prepare it for audio output.
    /// Called once on the control thread before starting the audio thread.
    virtual VoidResult Initialize(const AudioFormat& requestedFormat) = 0;

    /// Begin the audio clock. Called after Initialize().
    virtual VoidResult Start() = 0;

    /// Stop audio output. May be called while the audio thread is running;
    /// implementations must handle concurrent Stop()/Write() calls safely.
    virtual VoidResult Stop() = 0;

    /// Release all resources. Called after Stop().
    virtual void Shutdown() = 0;

    // -------------------------------------------------------------------------
    // Audio path
    // -------------------------------------------------------------------------

    /// Write one buffer of interleaved PCM audio (IEEE float or int).
    ///
    /// @param data         Pointer to sample data
    /// @param frameCount   Number of audio frames in data
    /// @returns            VoidResult::ok() if consumed, error otherwise.
    ///
    /// CONTRACT: This function MUST return within one buffer period.
    ///           It MUST NOT allocate heap memory.
    ///           It MUST NOT block indefinitely.
    virtual VoidResult Write(const float* data, uint32_t frameCount) = 0;

    // -------------------------------------------------------------------------
    // Query
    // -------------------------------------------------------------------------

    /// Returns the DeviceInfo snapshot for this device.
    virtual const DeviceInfo& GetDeviceInfo() const = 0;

    /// Returns the actual format negotiated with the driver after Initialize().
    virtual const AudioFormat& GetActualFormat() const = 0;

    /// Current playback position in frames (monotonically increasing).
    virtual uint64_t GetPosition() const = 0;

    /// True if the device is currently active and consuming audio.
    virtual bool IsRunning() const = 0;

    /// Estimated latency introduced by this device's driver + hardware (ms).
    virtual double GetLatencyMs() const = 0;
};

} // namespace var
