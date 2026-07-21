#pragma once

// =============================================================================
// Logger.h — Virtual Audio Router
// =============================================================================
// Asynchronous, thread-safe logger with in-memory ring buffer.
//
// RESPONSIBILITY:
//   Accept log messages from ANY thread without blocking the caller. The actual
//   writing to disk and the in-memory ring buffer is done on a dedicated Logger
//   thread to ensure audio threads are never stalled by disk I/O.
//
// INPUTS:
//   - Log level (Debug/Info/Warning/Error/Fatal)
//   - Message string
//   - Source location (automatically captured via macro)
//
// OUTPUTS:
//   - Appends to "var_engine.log" in the application data directory
//   - Maintains a fixed-size ring buffer readable from Python via GetRecentLogs()
//
// DEPENDENCIES:
//   - core/Types.h (LogEntry, LogLevel)
//   - core/Constants.h (kLoggerQueueSize, kMaxInMemoryLogEntries)
//
// THREADING:
//   - Callers post to a std::queue<LogEntry> under a std::mutex.
//   - A std::condition_variable wakes the Logger thread when messages arrive.
//   - Logger thread drains the queue and writes to disk + ring buffer.
//   - No heap allocation happens on the audio thread hot path (Phase 5 upgrade).
//
// FUTURE:
//   - Upgrade internal queue to a lock-free SPSC/MPSC queue for audio threads.
//   - Add JSON-structured output for installer diagnostics.
//   - Add log rotation (max file size).
// =============================================================================

#include <string>
#include <string_view>
#include <queue>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <fstream>
#include <source_location>
#include "var/Types.h"
#include "var/Result.h"
#include "var/Constants.h"

namespace var {

class Logger {
public:
    Logger();
    ~Logger();

    // Non-copyable, non-movable (owns a thread)
    Logger(const Logger&)            = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&)                 = delete;
    Logger& operator=(Logger&&)      = delete;

    // -------------------------------------------------------------------------
    // Lifecycle
    // -------------------------------------------------------------------------

    /// Open log file and start the background writer thread.
    /// @param logDirectory  Directory where var_engine.log will be created.
    VoidResult Initialize(const std::string& logDirectory);

    /// Flush all pending messages and stop the writer thread.
    void Shutdown();

    // -------------------------------------------------------------------------
    // Logging API
    // -------------------------------------------------------------------------

    void Log(LogLevel level, std::string_view message,
             const std::source_location& loc = std::source_location::current());

    void Debug(std::string_view msg,
               const std::source_location& loc = std::source_location::current()) {
        Log(LogLevel::Debug, msg, loc);
    }
    void Info(std::string_view msg,
              const std::source_location& loc = std::source_location::current()) {
        Log(LogLevel::Info, msg, loc);
    }
    void Warning(std::string_view msg,
                 const std::source_location& loc = std::source_location::current()) {
        Log(LogLevel::Warning, msg, loc);
    }
    void Error(std::string_view msg,
               const std::source_location& loc = std::source_location::current()) {
        Log(LogLevel::Error, msg, loc);
    }
    void Fatal(std::string_view msg,
               const std::source_location& loc = std::source_location::current()) {
        Log(LogLevel::Fatal, msg, loc);
    }

    // -------------------------------------------------------------------------
    // Python interface
    // -------------------------------------------------------------------------

    /// Returns up to `maxEntries` recent log entries (newest last).
    /// Called from Python to populate the log viewer panel.
    /// Thread-safe.
    std::vector<LogEntry> GetRecentLogs(uint32_t maxEntries = 100) const;

    /// Set the minimum log level; messages below this are discarded.
    void SetLevel(LogLevel level) { m_minLevel.store(level); }

    LogLevel GetLevel() const { return m_minLevel.load(); }

private:
    void WriterThreadFunc();
    void FlushEntry(const LogEntry& entry);
    std::string FormatEntry(const LogEntry& entry) const;
    int64_t NowMs() const;

    // Internal queue (producer side — any thread)
    mutable std::mutex              m_queueMutex;
    std::condition_variable         m_cv;
    std::queue<LogEntry>            m_queue;

    // Ring buffer (consumer side — writer thread writes, Python reads)
    mutable std::mutex              m_ringMutex;
    std::deque<LogEntry>            m_ring;        ///< bounded by kMaxInMemoryLogEntries
    uint32_t                        m_maxRingSize { constants::kMaxInMemoryLogEntries };

    // Writer thread
    std::thread                     m_writerThread;
    std::atomic<bool>               m_running     { false };
    std::atomic<LogLevel>           m_minLevel    { LogLevel::Debug };

    // File output
    std::ofstream                   m_logFile;

    // Timing baseline
    std::chrono::steady_clock::time_point m_startTime;
    bool m_initialized { false };
};

} // namespace var
