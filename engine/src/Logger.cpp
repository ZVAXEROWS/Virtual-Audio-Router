// =============================================================================
// Logger.cpp — Virtual Audio Router
// =============================================================================

#include "var/Logger.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <chrono>
#include <ctime>

namespace var {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static const char* LevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::Debug:   return "DEBUG";
        case LogLevel::Info:    return "INFO ";
        case LogLevel::Warning: return "WARN ";
        case LogLevel::Error:   return "ERROR";
        case LogLevel::Fatal:   return "FATAL";
        default:                return "?????";
    }
}

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

Logger::Logger() = default;

Logger::~Logger() {
    if (m_running.load()) {
        Shutdown();
    }
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

VoidResult Logger::Initialize(const std::string& logDirectory) {
    if (m_initialized) {
        return VoidResult::err(VarError{ ErrorCode::AlreadyInitialized,
                                         "Logger already initialized" });
    }

    // Create directory if it doesn't exist
    std::error_code ec;
    std::filesystem::create_directories(logDirectory, ec);
    if (ec) {
        return VoidResult::err(VarError{ ErrorCode::LogOpenFailed,
                                         "Cannot create log directory: " + ec.message() });
    }

    // Open log file (append mode so we don't lose history on restart)
    std::string logPath = (std::filesystem::path(logDirectory) /
                           std::string(constants::kLogFilename)).string();
    m_logFile.open(logPath, std::ios::app);
    if (!m_logFile.is_open()) {
        return VoidResult::err(VarError{ ErrorCode::LogOpenFailed,
                                         "Cannot open log file: " + logPath });
    }

    m_startTime   = std::chrono::steady_clock::now();
    m_initialized = true;
    m_running.store(true);

    // Start background writer thread
    m_writerThread = std::thread(&Logger::WriterThreadFunc, this);

    // Log the first entry directly (bootstrapping)
    Log(LogLevel::Info, "Logger initialized. Log file: " + logPath);

    return VoidResult::ok();
}

void Logger::Shutdown() {
    if (!m_running.load()) return;

    // Signal shutdown and wake the writer
    {
        std::lock_guard lock(m_queueMutex);
        m_running.store(false);
    }
    m_cv.notify_all();

    if (m_writerThread.joinable()) {
        m_writerThread.join();
    }

    if (m_logFile.is_open()) {
        m_logFile.close();
    }

    m_initialized = false;
}

// ---------------------------------------------------------------------------
// Logging
// ---------------------------------------------------------------------------

void Logger::Log(LogLevel level, std::string_view message,
                 const std::source_location& loc) {
    if (level < m_minLevel.load()) return;

    LogEntry entry;
    entry.level       = level;
    entry.message     = std::string(message);
    entry.source      = std::string(loc.file_name()) + ":" +
                        std::to_string(loc.line());
    entry.timestampMs = NowMs();

    // Trim source path to just filename for readability
    auto slashPos = entry.source.rfind('\\');
    if (slashPos == std::string::npos) slashPos = entry.source.rfind('/');
    if (slashPos != std::string::npos) {
        entry.source = entry.source.substr(slashPos + 1);
    }

    {
        std::lock_guard lock(m_queueMutex);
        m_queue.push(std::move(entry));
        // Safety: prevent unbounded growth if writer thread is slow
        if (m_queue.size() > constants::kLoggerQueueSize) {
            m_queue.pop();  // drop oldest (never stall caller)
        }
    }
    m_cv.notify_one();
}

// ---------------------------------------------------------------------------
// Python interface
// ---------------------------------------------------------------------------

std::vector<LogEntry> Logger::GetRecentLogs(uint32_t maxEntries) const {
    std::lock_guard lock(m_ringMutex);
    uint32_t count = std::min(maxEntries, static_cast<uint32_t>(m_ring.size()));
    uint32_t start = static_cast<uint32_t>(m_ring.size()) - count;
    return std::vector<LogEntry>(m_ring.begin() + start, m_ring.end());
}

// ---------------------------------------------------------------------------
// Writer thread
// ---------------------------------------------------------------------------

void Logger::WriterThreadFunc() {
    while (true) {
        std::unique_lock lock(m_queueMutex);
        m_cv.wait(lock, [this] {
            return !m_queue.empty() || !m_running.load();
        });

        // Drain the queue
        while (!m_queue.empty()) {
            LogEntry entry = std::move(m_queue.front());
            m_queue.pop();
            lock.unlock();

            FlushEntry(entry);

            lock.lock();
        }

        if (!m_running.load() && m_queue.empty()) {
            break;
        }
    }
}

void Logger::FlushEntry(const LogEntry& entry) {
    std::string formatted = FormatEntry(entry);

    // Write to file
    if (m_logFile.is_open()) {
        m_logFile << formatted << '\n';
        m_logFile.flush();
    }

    // Echo to stderr in debug builds
#ifndef NDEBUG
    std::cerr << formatted << '\n';
#endif

    // Add to in-memory ring buffer
    {
        std::lock_guard lock(m_ringMutex);
        m_ring.push_back(entry);
        while (m_ring.size() > m_maxRingSize) {
            m_ring.pop_front();
        }
    }
}

std::string Logger::FormatEntry(const LogEntry& entry) const {
    std::ostringstream oss;
    oss << '['
        << std::setw(8) << std::setfill(' ') << entry.timestampMs
        << "ms] ["
        << LevelToString(entry.level)
        << "] ["
        << std::setw(24) << std::setfill(' ') << std::left << entry.source
        << "] "
        << entry.message;
    return oss.str();
}

int64_t Logger::NowMs() const {
    if (!m_initialized) return 0;
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - m_startTime
    ).count();
}

} // namespace var
