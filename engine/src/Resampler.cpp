// =============================================================================
// Resampler.cpp — Virtual Audio Router
// =============================================================================
// Phase 1: Stub. Phase 3: Linear interpolation. Phase 5: Windowed sinc.
// =============================================================================

#include "var/Resampler.h"
#include "var/Logger.h"

namespace var {

struct Resampler::Impl {};

Resampler::Resampler(Logger& logger)
    : m_impl(std::make_unique<Impl>())
    , m_logger(logger)
{}

Resampler::~Resampler() { Shutdown(); }

VoidResult Resampler::Initialize(const AudioFormat& sourceFormat, uint32_t targetSampleRate) {
    m_sourceFormat  = sourceFormat;
    m_targetRate    = targetSampleRate;
    m_passthrough   = (sourceFormat.sampleRate == targetSampleRate);
    m_initialized   = true;
    m_logger.Info("Resampler: " +
                  std::to_string(sourceFormat.sampleRate) + " -> " +
                  std::to_string(targetSampleRate) + " Hz. Passthrough=" +
                  (m_passthrough ? "true" : "false") + " (Phase 1 stub).");
    return VoidResult::ok();
}

void Resampler::Shutdown() {
    m_initialized = false;
}

VoidResult Resampler::Process(const float* inData, uint32_t inFrameCount,
                               float* outData, uint32_t& outFrameCount) {
    if (m_passthrough) {
        std::copy(inData, inData + inFrameCount * m_sourceFormat.channels, outData);
        outFrameCount = inFrameCount;
        return VoidResult::ok();
    }
    // Phase 3: implement linear interpolation here
    outFrameCount = 0;
    return VoidResult::err(VarError{ ErrorCode::Unknown,
                                      "Resampler not implemented yet (Phase 3)" });
}

uint32_t Resampler::OutputFrameCount(uint32_t inputFrames) const {
    if (m_passthrough || m_sourceFormat.sampleRate == 0) return inputFrames;
    return static_cast<uint32_t>(
        static_cast<double>(inputFrames) * m_targetRate / m_sourceFormat.sampleRate + 0.5);
}

} // namespace var
