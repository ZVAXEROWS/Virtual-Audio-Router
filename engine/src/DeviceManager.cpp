// =============================================================================
// DeviceManager.cpp — Virtual Audio Router
// =============================================================================
// Phase 1: COM is initialized; EnumerateDevices returns an empty vector.
// Phase 2: Real WASAPI IMMDeviceEnumerator implementation fills this in.
// =============================================================================

// Windows headers must come before standard headers to avoid conflicts
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>

#include "var/DeviceManager.h"
#include "var/Logger.h"
#include "var/EventDispatcher.h"

namespace var {

// ---------------------------------------------------------------------------
// Impl — COM objects, hidden from the header
// ---------------------------------------------------------------------------

struct DeviceManager::Impl {
    // Phase 2: IMMDeviceEnumerator*, IMMNotificationClient* go here
    bool comInitialized { false };
};

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

DeviceManager::DeviceManager(Logger& logger, EventDispatcher& dispatcher)
    : m_impl(std::make_unique<Impl>())
    , m_logger(logger)
    , m_dispatcher(dispatcher)
{}

DeviceManager::~DeviceManager() {
    if (m_initialized) {
        Shutdown();
    }
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

VoidResult DeviceManager::Initialize() {
    if (m_initialized) {
        return VoidResult::err(VarError{ ErrorCode::AlreadyInitialized,
                                         "DeviceManager already initialized" });
    }

    m_logger.Info("DeviceManager: Initializing COM...");

    // Initialize COM on this thread (STA — required for MMDevice API)
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        return VoidResult::err(VarError::fromHresult(
            ErrorCode::ComInitFailed, static_cast<uint32_t>(hr),
            "CoInitializeEx failed"));
    }

    m_impl->comInitialized = true;
    m_initialized = true;

    m_logger.Info("DeviceManager: COM initialized successfully (Phase 1 stub).");
    m_logger.Info("DeviceManager: WASAPI enumeration will be implemented in Phase 2.");

    return VoidResult::ok();
}

void DeviceManager::Shutdown() {
    if (!m_initialized) return;

    m_logger.Info("DeviceManager: Shutting down...");

    if (m_impl->comInitialized) {
        CoUninitialize();
        m_impl->comInitialized = false;
    }

    m_initialized = false;
    m_logger.Info("DeviceManager: Shutdown complete.");
}

// ---------------------------------------------------------------------------
// Enumeration (Phase 1 stubs — Phase 2 replaces these)
// ---------------------------------------------------------------------------

Result<std::vector<DeviceInfo>, VarError> DeviceManager::EnumerateDevices(DeviceType type) {
    if (!m_initialized) {
        return Result<std::vector<DeviceInfo>, VarError>::err(
            VarError{ ErrorCode::EngineNotInitialized,
                      "DeviceManager not initialized" });
    }

    m_logger.Info("DeviceManager: EnumerateDevices called (Phase 1 stub — returning empty list).");

    // Phase 2: instantiate IMMDeviceEnumerator and walk IMMDeviceCollection
    return Result<std::vector<DeviceInfo>, VarError>::ok({});
}

Result<DeviceInfo, VarError> DeviceManager::GetDeviceById(const std::string& deviceId) {
    if (!m_initialized) {
        return Result<DeviceInfo, VarError>::err(
            VarError{ ErrorCode::EngineNotInitialized,
                      "DeviceManager not initialized" });
    }

    m_logger.Info("DeviceManager: GetDeviceById called for: " + deviceId +
                  " (Phase 1 stub).");
    return Result<DeviceInfo, VarError>::err(
        VarError{ ErrorCode::DeviceNotFound, "Phase 1 stub — no devices enumerated" });
}

Result<DeviceInfo, VarError> DeviceManager::GetDefaultDevice(DeviceType type) {
    if (!m_initialized) {
        return Result<DeviceInfo, VarError>::err(
            VarError{ ErrorCode::EngineNotInitialized,
                      "DeviceManager not initialized" });
    }

    m_logger.Info("DeviceManager: GetDefaultDevice called (Phase 1 stub).");
    return Result<DeviceInfo, VarError>::err(
        VarError{ ErrorCode::DeviceNotFound, "Phase 1 stub — no devices enumerated" });
}

// ---------------------------------------------------------------------------
// Monitoring (Phase 1 stubs)
// ---------------------------------------------------------------------------

VoidResult DeviceManager::StartMonitoring() {
    m_logger.Info("DeviceManager: StartMonitoring called (Phase 1 stub).");
    return VoidResult::ok();
}

void DeviceManager::StopMonitoring() {
    m_logger.Info("DeviceManager: StopMonitoring called (Phase 1 stub).");
}

} // namespace var
