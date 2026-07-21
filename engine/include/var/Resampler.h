#pragma once

// =============================================================================
// Resampler.h — Virtual Audio Router
// =============================================================================
// Converts audio between different sample rates.
//
// RESPONSIBILITY:
//   When an output device's native format (e.g. 44100 Hz) differs from the
//   engine's internal mixing format (48000 Hz), the Resampler converts the
//   buffer on the fly before Write() is called.
//
// INPUTS:
//   - Source audio: AudioFormat (sampleRate, channels, frames)
//   - Target sample rate
//   - PCM float frames
//
// OUTPUTS:
//   - Resampled PCM float frames at the target rate
//
// ALGORITHM:
//   Phase 3: Linear interpolation (acceptable quality, very fast)
//   Phase 5: Windowed sinc resampler (studio-grade quality)
//   Phase 7: Option to use Windows Resampler DMO (system-provided, good quality)
//
// PHASE 1: Stub header only.
// =============================================================================

#include <cstdint>
#include <vector>
#include <memory>
#include "var/Types.h"
#include "var/Result.h"

namespace var {

class Logger;

class Resampler {
public:
    explicit Resampler(Logger& logger);
    ~Resampler();

    Resampler(const Resampler&)            = delete;
    Resampler& operator=(const Resampler&) = delete;

    // -------------------------------------------------------------------------
    // Lifecycle
    // -------------------------------------------------------------------------

    VoidResult Initialize(const AudioFormat& sourceFormat, uint32_t targetSampleRate);
    void       Shutdown();

    // -------------------------------------------------------------------------
    // Processing
    // -------------------------------------------------------------------------

    /// Convert `frameCount` frames from sourceFormat to targetSampleRate.
    /// Output is written to `outBuffer` (caller-allocated, sized via OutputFrameCount).
    VoidResult Process(const float* inData,  uint32_t inFrameCount,
                             float* outData, uint32_t& outFrameCount);

    /// How many output frames will Process() produce for `inputFrames` of input?
    uint32_t OutputFrameCount(uint32_t inputFrames) const;

    bool IsPassthrough() const { return m_passthrough; }

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    AudioFormat m_sourceFormat {};
    uint32_t    m_targetRate   { 48000 };
    bool        m_passthrough  { true };   ///< true when source == target rate
    bool        m_initialized  { false };
    Logger&     m_logger;
};

} // namespace var
