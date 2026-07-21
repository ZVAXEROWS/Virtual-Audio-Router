// =============================================================================
// test_logger.cpp — Unit tests for async Logger
// =============================================================================

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <filesystem>
#include "var/Logger.h"

using namespace var;
using namespace std::chrono_literals;

class LoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Use a temp directory under the build output
        m_logDir = std::filesystem::temp_directory_path().string() + "/var_test_logs";
        std::filesystem::create_directories(m_logDir);
    }
    void TearDown() override {
        if (m_logger) {
            m_logger->Shutdown();
        }
        std::filesystem::remove_all(m_logDir);
    }

    std::unique_ptr<Logger> m_logger;
    std::string             m_logDir;
};

TEST_F(LoggerTest, InitializeSucceeds) {
    m_logger = std::make_unique<Logger>();
    auto result = m_logger->Initialize(m_logDir);
    EXPECT_TRUE(result.isOk()) << result.error().message;
}

TEST_F(LoggerTest, LogFileCreated) {
    m_logger = std::make_unique<Logger>();
    m_logger->Initialize(m_logDir);

    std::filesystem::path logPath =
        std::filesystem::path(m_logDir) / std::string(constants::kLogFilename);
    // Give writer thread time to flush
    std::this_thread::sleep_for(100ms);
    EXPECT_TRUE(std::filesystem::exists(logPath));
}

TEST_F(LoggerTest, MessagesAppearsInRingBuffer) {
    m_logger = std::make_unique<Logger>();
    m_logger->Initialize(m_logDir);

    m_logger->Info("Hello from test");
    m_logger->Warning("This is a warning");
    m_logger->Error("This is an error");

    // Wait for writer thread
    std::this_thread::sleep_for(100ms);

    auto logs = m_logger->GetRecentLogs(50);
    ASSERT_GE(logs.size(), 3u);

    bool foundInfo    = false;
    bool foundWarning = false;
    bool foundError   = false;
    for (const auto& entry : logs) {
        if (entry.message.find("Hello from test") != std::string::npos)
            foundInfo = true;
        if (entry.message.find("This is a warning") != std::string::npos)
            foundWarning = true;
        if (entry.message.find("This is an error") != std::string::npos)
            foundError = true;
    }
    EXPECT_TRUE(foundInfo);
    EXPECT_TRUE(foundWarning);
    EXPECT_TRUE(foundError);
}

TEST_F(LoggerTest, LevelFilterWorks) {
    m_logger = std::make_unique<Logger>();
    m_logger->Initialize(m_logDir);
    m_logger->SetLevel(LogLevel::Warning);

    m_logger->Debug("should be filtered");
    m_logger->Info("should also be filtered");
    m_logger->Warning("should appear");

    std::this_thread::sleep_for(100ms);

    auto logs = m_logger->GetRecentLogs(50);
    bool foundDebug   = false;
    bool foundWarning = false;
    for (const auto& entry : logs) {
        if (entry.message.find("should be filtered") != std::string::npos)
            foundDebug = true;
        if (entry.message.find("should appear") != std::string::npos)
            foundWarning = true;
    }
    EXPECT_FALSE(foundDebug);
    EXPECT_TRUE(foundWarning);
}

TEST_F(LoggerTest, MultiThreadedLoggingDoesNotCrash) {
    m_logger = std::make_unique<Logger>();
    m_logger->Initialize(m_logDir);

    constexpr int kThreads  = 8;
    constexpr int kMessages = 100;

    std::vector<std::thread> threads;
    threads.reserve(kThreads);

    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([this, t]() {
            for (int i = 0; i < kMessages; ++i) {
                m_logger->Info("Thread " + std::to_string(t) +
                               " message " + std::to_string(i));
            }
        });
    }
    for (auto& th : threads) th.join();

    std::this_thread::sleep_for(200ms);
    // If we get here without crash or deadlock, test passes
    EXPECT_TRUE(true);
}

TEST_F(LoggerTest, GetRecentLogsRespectsMaxEntries) {
    m_logger = std::make_unique<Logger>();
    m_logger->Initialize(m_logDir);

    for (int i = 0; i < 50; ++i) {
        m_logger->Info("Message " + std::to_string(i));
    }
    std::this_thread::sleep_for(150ms);

    auto logs = m_logger->GetRecentLogs(10);
    EXPECT_LE(logs.size(), 10u);
}

TEST_F(LoggerTest, DoubleInitializeReturnsError) {
    m_logger = std::make_unique<Logger>();
    EXPECT_TRUE(m_logger->Initialize(m_logDir).isOk());
    EXPECT_FALSE(m_logger->Initialize(m_logDir).isOk());
}
