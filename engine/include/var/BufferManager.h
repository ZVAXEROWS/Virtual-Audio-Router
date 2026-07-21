#pragma once

// =============================================================================
// BufferManager.h — Virtual Audio Router
// =============================================================================
// Manages the shared audio buffer between the capture side and all output devices.
//
// RESPONSIBILITY:
//   Provide a thread-safe ring buffer that:
//     - The capture thread writes audio frames into
//     - Each output thread reads audio frames from independently
//
// WHY PER-READER READ POSITIONS:
//   Each output device has a different read position (its own cursor into the
//   ring buffer). This avoids copying audio N times — all output threads read
//   from the same buffer but track their own positions.
//
// INPUTS:
//   - Audio frames pushed by the capture thread (Write)
//   - Audio frames pulled by output threads (Read)
//
// OUTPUTS:
//   - Interleaved PCM float frames
//
// PHASE 1: Interface and stub. No ring buffer implementation yet.
// PHASE 4: Full multi-reader ring buffer implemented.
// =============================================================================

#include <cstdint>
#include <vector>
#include <memory>
#include "var/Types.h"
#include "var/Result.h"

namespace var {

class Logger;

class BufferManager {
public:
    explicit BufferManager(Logger& logger);
    ~BufferManager();

    BufferManager(const BufferManager&)            = delete;
    BufferManager& operator=(const BufferManager&) = delete;

    // -------------------------------------------------------------------------
    // Lifecycle
    // -------------------------------------------------------------------------

    /// Allocate the ring buffer with capacity for at least `capacityMs` of audio.
    VoidResult Initialize(const AudioFormat& format, uint32_t capacityMs = 200);

    void Shutdown();

    // -------------------------------------------------------------------------
    // Writer interface (capture thread)
    // -------------------------------------------------------------------------

    /// Write frameCount frames to the ring buffer.
    /// Returns error if the buffer would overflow.
    VoidResult Write(const float* data, uint32_t frameCount);

    // -------------------------------------------------------------------------
    // Reader interface (output threads)
    // -------------------------------------------------------------------------

    /// Register a new reader; returns a reader ID.
    uint32_t RegisterReader();

    /// Read frameCount frames for a given reader.
    /// If not enough data, returns zeros (underrun).
    VoidResult Read(uint32_t readerId, float* outData, uint32_t frameCount);

    /// Number of frames available to read for a given reader.
    uint32_t Available(uint32_t readerId) const;

    // -------------------------------------------------------------------------
    // State
    // -------------------------------------------------------------------------

    const AudioFormat& GetFormat()     const { return m_format; }
    uint32_t           GetCapacity()   const { return m_capacityFrames; }
    bool               IsInitialized() const { return m_initialized; }

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    AudioFormat m_format {};
    uint32_t    m_capacityFrames { 0 };
    bool        m_initialized    { false };
    Logger&     m_logger;
};

} // namespace var
