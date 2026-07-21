// =============================================================================
// AudioEngine.cpp — Virtual Audio Router
// =============================================================================
// Top-level orchestrator. Owns all subsystems. Drives the state machine.
// =============================================================================

#include "var/AudioEngine.h"
#include "var/Logger.h"
#include "var/EventDispatcher.h"
#include "var/ThreadPool.h"
#include "var/DeviceManager.h"
#include "var/BufferManager.h"
#include "var/LatencyManager.h"
#include "var/SynchronizationManager.h"
#include "var/AudioRouter.h"
#include "var/OutputDevice.h"
#include "var/Constants.h"

#include <filesystem>
#include <mutex>

namespace var {

// ---------------------------------------------------------------------------
// Impl — private member storage
// ---------------------------------------------------------------------------

struct AudioEngine::Impl {
    // Subsystems are created in Initialize() order and destroyed in reverse
    // by the unique_ptr chain. Logger is always first/last.
    std::unique_ptr<Logger>                m_logger;
    std::unique_ptr<EventDispatcher>       m_dispatcher;
    std::unique_ptr<ThreadPool>            m_threadPool;
    std::unique_ptr<DeviceManager>         m_deviceManager;
    std::unique_ptr<BufferManager>         m_bufferManager;
    std::unique_ptr<LatencyManager>        m_latencyManager;
    std::unique_ptr<SynchronizationManager> m_syncManager;
    std::unique_ptr<AudioRouter>           m_router;

    RouterConfig                           m_currentConfig;
    mutable std::mutex                     m_configMutex;

    std::vector<std::unique_ptr<IDevice>>  m_activeOutputs;
};

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

AudioEngine::AudioEngine()
    : m_impl(std::make_unique<Impl>())
    , m_status(EngineStatus::Uninitialized)
{}

AudioEngine::~AudioEngine() {
    if (m_status.load() != EngineStatus::Uninitialized &&
        m_status.load() != EngineStatus::ShuttingDown) {
        Shutdown();
    }
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

VoidResult AudioEngine::Initialize(const std::string& logDirectory) {
    if (m_status.load() != EngineStatus::Uninitialized) {
        return VoidResult::err(VarError{ ErrorCode::AlreadyInitialized,
                                         "AudioEngine already initialized" });
    }

    SetStatus(EngineStatus::Initializing);

    // --- 1. Logger (must be first — everything else logs) -------------------
    m_impl->m_logger = std::make_unique<Logger>();
    auto logResult = m_impl->m_logger->Initialize(logDirectory);
    if (!logResult) {
        SetStatus(EngineStatus::Error);
        return logResult;
    }

    auto& log = *m_impl->m_logger;
    log.Info("=== Virtual Audio Router v" +
             std::string(constants::kVersionString) + " ===");
    log.Info("AudioEngine: Initializing...");

    // --- 2. EventDispatcher -------------------------------------------------
    m_impl->m_dispatcher = std::make_unique<EventDispatcher>();
    log.Info("AudioEngine: EventDispatcher created.");

    // --- 3. ThreadPool -------------------------------------------------------
    m_impl->m_threadPool = std::make_unique<ThreadPool>(constants::kThreadPoolSize);
    log.Info("AudioEngine: ThreadPool started with " +
             std::to_string(constants::kThreadPoolSize) + " threads.");

    // --- 4. DeviceManager ---------------------------------------------------
    m_impl->m_deviceManager = std::make_unique<DeviceManager>(log, *m_impl->m_dispatcher);
    auto devResult = m_impl->m_deviceManager->Initialize();
    if (!devResult) {
        log.Error("AudioEngine: DeviceManager init failed: " + devResult.error().message);
        SetStatus(EngineStatus::Error);
        return devResult;
    }
    m_impl->m_deviceManager->StartMonitoring();

    // --- 5. BufferManager ---------------------------------------------------
    m_impl->m_bufferManager = std::make_unique<BufferManager>(log);
    AudioFormat defaultFormat{};
    auto bufResult = m_impl->m_bufferManager->Initialize(defaultFormat, 200);
    if (!bufResult) {
        log.Error("AudioEngine: BufferManager init failed: " + bufResult.error().message);
        SetStatus(EngineStatus::Error);
        return bufResult;
    }

    // --- 6. LatencyManager --------------------------------------------------
    m_impl->m_latencyManager = std::make_unique<LatencyManager>(log);
    m_impl->m_latencyManager->Initialize();

    // --- 7. SynchronizationManager ------------------------------------------
    m_impl->m_syncManager = std::make_unique<SynchronizationManager>(log);
    m_impl->m_syncManager->Initialize();

    // --- 8. AudioRouter -----------------------------------------------------
    m_impl->m_router = std::make_unique<AudioRouter>(
        log, *m_impl->m_bufferManager, *m_impl->m_latencyManager);
    m_impl->m_router->Initialize();

    // --- Done ---------------------------------------------------------------
    SetStatus(EngineStatus::Ready);
    log.Info("AudioEngine: All subsystems initialized. Status=Ready.");

    m_impl->m_dispatcher->Publish(EvEngineInitialized{});

    return VoidResult::ok();
}

void AudioEngine::Shutdown() {
    SetStatus(EngineStatus::ShuttingDown);

    if (m_impl->m_logger)
        m_impl->m_logger->Info("AudioEngine: Shutting down all subsystems...");

    if (m_impl->m_router)         m_impl->m_router->Shutdown();
    if (m_impl->m_syncManager)    m_impl->m_syncManager->Shutdown();
    if (m_impl->m_latencyManager) m_impl->m_latencyManager->Shutdown();
    if (m_impl->m_bufferManager)  m_impl->m_bufferManager->Shutdown();
    if (m_impl->m_deviceManager) {
        m_impl->m_deviceManager->StopMonitoring();
        m_impl->m_deviceManager->Shutdown();
    }
    if (m_impl->m_threadPool)     m_impl->m_threadPool->Shutdown();
    if (m_impl->m_dispatcher)     m_impl->m_dispatcher->Publish(EvEngineShutdown{});
    if (m_impl->m_dispatcher)     m_impl->m_dispatcher->Clear();

    if (m_impl->m_logger) {
        m_impl->m_logger->Info("AudioEngine: Shutdown complete.");
        m_impl->m_logger->Shutdown();
    }

    SetStatus(EngineStatus::Uninitialized);
}

void AudioEngine::SetDeviceChangeCallback(std::function<void()> callback) {
    if (m_impl && m_impl->m_deviceManager) {
        m_impl->m_deviceManager->SetDeviceChangeCallback(std::move(callback));
    }
}

// ---------------------------------------------------------------------------
// Device management
// ---------------------------------------------------------------------------

Result<std::vector<DeviceInfo>, VarError> AudioEngine::GetDevices() {
    if (!m_impl->m_deviceManager) {
        return Result<std::vector<DeviceInfo>, VarError>::err(
            VarError{ ErrorCode::EngineNotInitialized, "Engine not initialized" });
    }
    return m_impl->m_deviceManager->EnumerateDevices(DeviceType::Render);
}

Result<DeviceInfo, VarError> AudioEngine::GetDefaultDevice() {
    if (!m_impl->m_deviceManager) {
        return Result<DeviceInfo, VarError>::err(
            VarError{ ErrorCode::EngineNotInitialized, "Engine not initialized" });
    }
    return m_impl->m_deviceManager->GetDefaultDevice();
}

// ---------------------------------------------------------------------------
// Routing
// ---------------------------------------------------------------------------

VoidResult AudioEngine::StartRouting(const RouterConfig& config) {
    if (m_status.load() != EngineStatus::Ready) {
        return VoidResult::err(VarError{ ErrorCode::InvalidConfig,
                                         "Engine must be in Ready state to start routing" });
    }

    {
        std::lock_guard lock(m_impl->m_configMutex);
        m_impl->m_currentConfig = config;
    }

    m_impl->m_logger->Info("AudioEngine: StartRouting called. " +
                           std::to_string(config.outputDeviceIds.size()) +
                           " output device(s) requested.");

    // 1. Resolve output devices
    std::vector<IDevice*> outputPointers;
    m_impl->m_activeOutputs.clear();

    for (const auto& id : config.outputDeviceIds) {
        auto infoResult = m_impl->m_deviceManager->GetDeviceById(id);
        if (infoResult) {
            auto outDev = std::make_unique<OutputDevice>(infoResult.value(), *m_impl->m_logger);
            outputPointers.push_back(outDev.get());
            m_impl->m_activeOutputs.push_back(std::move(outDev));
        }
    }

    m_impl->m_router->ConfigureOutputs(std::move(outputPointers));
    auto routeRes = m_impl->m_router->RouteBuffer(0);
    if (!routeRes) {
        return VoidResult::err(VarError{ErrorCode::StartFailed, "Failed to start capture: " + routeRes.error().message});
    }

    SetStatus(EngineStatus::Routing);
    m_impl->m_dispatcher->Publish(EvRoutingStarted{
        config.inputDeviceId, config.outputDeviceIds });

    return VoidResult::ok();
}

VoidResult AudioEngine::StopRouting() {
    if (m_status.load() != EngineStatus::Routing) {
        return VoidResult::err(VarError{ ErrorCode::StopFailed,
                                         "Engine is not currently routing" });
    }

    m_impl->m_logger->Info("AudioEngine: StopRouting called.");
    SetStatus(EngineStatus::Stopping);

    m_impl->m_router->Shutdown();
    m_impl->m_activeOutputs.clear();

    SetStatus(EngineStatus::Ready);
    m_impl->m_dispatcher->Publish(EvRoutingStopped{});

    return VoidResult::ok();
}

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

std::vector<LogEntry> AudioEngine::GetRecentLogs(uint32_t maxEntries) const {
    if (!m_impl->m_logger) return {};
    return m_impl->m_logger->GetRecentLogs(maxEntries);
}

RouterConfig AudioEngine::GetCurrentConfig() const {
    std::lock_guard lock(m_impl->m_configMutex);
    return m_impl->m_currentConfig;
}

// ---------------------------------------------------------------------------
// Internal
// ---------------------------------------------------------------------------

void AudioEngine::SetStatus(EngineStatus status) {
    m_status.store(status);
}

} // namespace var
