// =============================================================================
// DeviceManager.cpp — Virtual Audio Router
// =============================================================================
// Phase 2: Full WASAPI IMMDeviceEnumerator implementation.
//
// COM THREADING NOTE:
//   We call CoInitializeEx(COINIT_APARTMENTTHREADED) here. This is correct for
//   the control thread that calls Enumerate/GetDefault. Audio threads that need
//   COM (Phase 3) must also call CoInitializeEx on their own threads.
//
// WIDE STRING HANDLING:
//   All WASAPI strings are LPWSTR (UTF-16). We convert to UTF-8 std::string
//   using WideCharToMultiByte so pybind11 can pass them to Python cleanly.
//
// PROPERTY KEYS:
//   PKEY_Device_FriendlyName    → "Speakers (Realtek HD Audio)"
//   PKEY_Device_DeviceDesc      → "High Definition Audio Device"
//   PKEY_AudioEngine_DeviceFormat → WAVEFORMATEX blob (native mix format)
// =============================================================================


#include <initguid.h>        // Define GUIDs
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>
#include <propvarutil.h>
#include <wrl/client.h>      // Microsoft::WRL::ComPtr
#include <ks.h>              // KSIDENTIFIER base types
#include <ksmedia.h>         // KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, WAVEFORMATEXTENSIBLE

#include <string>
#include <vector>
#include <stdexcept>

#include "var/DeviceManager.h"
#include "var/Logger.h"
#include "var/EventDispatcher.h"

using Microsoft::WRL::ComPtr;

namespace var {

// ---------------------------------------------------------------------------
// Utility: LPWSTR → std::string (UTF-8)
// ---------------------------------------------------------------------------

static std::string WideToUtf8(LPCWSTR wide) {
    if (!wide || wide[0] == L'\0') return {};
    int needed = WideCharToMultiByte(CP_UTF8, 0, wide, -1,
                                     nullptr, 0, nullptr, nullptr);
    if (needed <= 0) return {};
    std::string result(needed - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide, -1,
                        result.data(), needed, nullptr, nullptr);
    return result;
}

// ---------------------------------------------------------------------------
// Utility: HRESULT → descriptive error string
// ---------------------------------------------------------------------------

static std::string HrToString(HRESULT hr) {
    char buf[64];
    snprintf(buf, sizeof(buf), "HRESULT=0x%08X", static_cast<unsigned>(hr));
    return buf;
}

// ---------------------------------------------------------------------------
// Utility: IMMDevice → DeviceInfo
// Reads all properties from IPropertyStore, opens IAudioClient for format.
// ---------------------------------------------------------------------------

static DeviceInfo DeviceInfoFromIMMDevice(IMMDevice* pDevice,
                                           bool isDefault,
                                           Logger& logger) {
    DeviceInfo info;

    // --- ID ---
    LPWSTR pwszId = nullptr;
    if (SUCCEEDED(pDevice->GetId(&pwszId)) && pwszId) {
        info.id = WideToUtf8(pwszId);
        CoTaskMemFree(pwszId);
    }

    // --- State ---
    DWORD state = DEVICE_STATE_NOTPRESENT;
    pDevice->GetState(&state);
    switch (state) {
        case DEVICE_STATE_ACTIVE:    info.state = DeviceState::Active;     break;
        case DEVICE_STATE_DISABLED:  info.state = DeviceState::Disabled;   break;
        case DEVICE_STATE_NOTPRESENT: info.state = DeviceState::NotPresent; break;
        case DEVICE_STATE_UNPLUGGED: info.state = DeviceState::Unplugged;  break;
        default:                     info.state = DeviceState::Unknown;    break;
    }

    info.isDefault = isDefault;
    info.type      = DeviceType::Render;

    // --- Properties ---
    ComPtr<IPropertyStore> pProps;
    if (SUCCEEDED(pDevice->OpenPropertyStore(STGM_READ, &pProps))) {

        // Friendly name: "Speakers (Realtek HD Audio)"
        PROPVARIANT varName;
        PropVariantInit(&varName);
        if (SUCCEEDED(pProps->GetValue(PKEY_Device_FriendlyName, &varName))
            && varName.vt == VT_LPWSTR) {
            info.name = WideToUtf8(varName.pwszVal);
        }
        PropVariantClear(&varName);

        // Device description: "High Definition Audio Device"
        PROPVARIANT varDesc;
        PropVariantInit(&varDesc);
        if (SUCCEEDED(pProps->GetValue(PKEY_Device_DeviceDesc, &varDesc))
            && varDesc.vt == VT_LPWSTR) {
            info.description = WideToUtf8(varDesc.pwszVal);
        }
        PropVariantClear(&varDesc);

        // Native mix format stored as WAVEFORMATEX blob
        // Key: PKEY_AudioEngine_DeviceFormat
        PROPVARIANT varFmt;
        PropVariantInit(&varFmt);
        if (SUCCEEDED(pProps->GetValue(PKEY_AudioEngine_DeviceFormat, &varFmt))
            && varFmt.vt == VT_BLOB
            && varFmt.blob.cbSize >= sizeof(WAVEFORMATEX)) {

            const auto* wfx = reinterpret_cast<const WAVEFORMATEX*>(
                varFmt.blob.pBlobData);

            info.nativeFormat.sampleRate    = wfx->nSamplesPerSec;
            info.nativeFormat.channels      = wfx->nChannels;
            info.nativeFormat.bitsPerSample = wfx->wBitsPerSample;
            info.nativeFormat.isFloat       = (wfx->wFormatTag == WAVE_FORMAT_IEEE_FLOAT);

            // For WAVEFORMATEXTENSIBLE, check the sub-format
            if (wfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE
                && wfx->cbSize >= 22) {
                const auto* wfext = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(wfx);
                info.nativeFormat.isFloat =
                    (wfext->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT);
                info.nativeFormat.bitsPerSample = wfext->Samples.wValidBitsPerSample;
            }
        }
        PropVariantClear(&varFmt);
    }

    // --- Latency via IAudioClient::GetDevicePeriod ---
    // Only do this for active devices to avoid errors on disabled/unplugged
    if (info.state == DeviceState::Active) {
        ComPtr<IAudioClient> pAudioClient;
        HRESULT hr = pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL,
                                        nullptr, &pAudioClient);
        if (SUCCEEDED(hr)) {
            REFERENCE_TIME defaultPeriod = 0, minPeriod = 0;
            hr = pAudioClient->GetDevicePeriod(&defaultPeriod, &minPeriod);
            if (SUCCEEDED(hr)) {
                // REFERENCE_TIME is in 100-nanosecond units
                info.latencyMs = static_cast<double>(defaultPeriod) / 10000.0;
            }

            // Also: fill in format if property store didn't have it
            if (info.nativeFormat.sampleRate == 0) {
                WAVEFORMATEX* pMixFmt = nullptr;
                if (SUCCEEDED(pAudioClient->GetMixFormat(&pMixFmt)) && pMixFmt) {
                    info.nativeFormat.sampleRate    = pMixFmt->nSamplesPerSec;
                    info.nativeFormat.channels      = pMixFmt->nChannels;
                    info.nativeFormat.bitsPerSample = pMixFmt->wBitsPerSample;
                    info.nativeFormat.isFloat       =
                        (pMixFmt->wFormatTag == WAVE_FORMAT_IEEE_FLOAT);
                    CoTaskMemFree(pMixFmt);
                }
            }
        }
    }

    // Fallback name
    if (info.name.empty()) {
        info.name = "(Unknown Device — " + info.id.substr(0, 12) + "...)";
    }

    return info;
}

class NotificationClient : public IMMNotificationClient {
public:
    NotificationClient(std::function<void()> cb) : m_cb(std::move(cb)) {}
    virtual ~NotificationClient() = default;

    // IUnknown
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&m_ref); }
    ULONG STDMETHODCALLTYPE Release() override {
        ULONG ulRef = InterlockedDecrement(&m_ref);
        if (0 == ulRef) delete this;
        return ulRef;
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (IID_IUnknown == riid || __uuidof(IMMNotificationClient) == riid) {
            *ppv = static_cast<IMMNotificationClient*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    // IMMNotificationClient
    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR, DWORD) override {
        if (m_cb) m_cb();
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR) override {
        if (m_cb) m_cb();
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR) override {
        if (m_cb) m_cb();
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow, ERole, LPCWSTR) override {
        if (m_cb) m_cb();
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR, const PROPERTYKEY) override {
        // Ignore property changes to avoid spam
        return S_OK;
    }

private:
    LONG m_ref { 1 };
    std::function<void()> m_cb;
};

// ---------------------------------------------------------------------------
// Impl — owns the COM enumerator
// ---------------------------------------------------------------------------

struct DeviceManager::Impl {
    ComPtr<IMMDeviceEnumerator> pEnumerator;
    bool comInitialized { false };
    std::function<void()> callback;
    NotificationClient* pClient { nullptr };
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
    if (m_initialized) Shutdown();
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

VoidResult DeviceManager::Initialize() {
    if (m_initialized) {
        return VoidResult::err(VarError{ ErrorCode::AlreadyInitialized,
                                         "DeviceManager already initialized" });
    }

    m_logger.Info("DeviceManager: Initializing COM (STA)...");

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        return VoidResult::err(VarError::fromHresult(
            ErrorCode::ComInitFailed,
            static_cast<uint32_t>(hr), "CoInitializeEx"));
    }
    m_impl->comInitialized = true;

    // Create the MMDevice enumerator — the root COM object for WASAPI
    hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        reinterpret_cast<void**>(m_impl->pEnumerator.GetAddressOf())
    );
    if (FAILED(hr)) {
        CoUninitialize();
        m_impl->comInitialized = false;
        return VoidResult::err(VarError::fromHresult(
            ErrorCode::ComInitFailed,
            static_cast<uint32_t>(hr), "CoCreateInstance(MMDeviceEnumerator)"));
    }

    m_initialized = true;
    m_logger.Info("DeviceManager: IMMDeviceEnumerator ready.");
    return VoidResult::ok();
}

void DeviceManager::Shutdown() {
    if (!m_initialized) return;

    m_logger.Info("DeviceManager: Shutting down...");
    if (m_impl->pClient && m_impl->pEnumerator) {
        m_impl->pEnumerator->UnregisterEndpointNotificationCallback(m_impl->pClient);
        m_impl->pClient->Release();
        m_impl->pClient = nullptr;
    }

    m_impl->pEnumerator.Reset();

    if (m_impl->comInitialized) {
        CoUninitialize();
        m_impl->comInitialized = false;
    }

    m_initialized = false;
    m_logger.Info("DeviceManager: Shutdown complete.");
}

void DeviceManager::SetDeviceChangeCallback(std::function<void()> callback) {
    m_impl->callback = std::move(callback);
}

// ---------------------------------------------------------------------------
// Enumeration
// ---------------------------------------------------------------------------

Result<std::vector<DeviceInfo>, VarError>
DeviceManager::EnumerateDevices(DeviceType type) {

    if (!m_initialized || !m_impl->pEnumerator) {
        return Result<std::vector<DeviceInfo>, VarError>::err(
            VarError{ ErrorCode::EngineNotInitialized,
                      "DeviceManager not initialized" });
    }

    // Resolve the EDataFlow for the requested type
    EDataFlow flow = (type == DeviceType::Render) ? eRender : eCapture;

    // Enumerate ALL states so the GUI shows disabled/unplugged devices too
    DWORD stateMask = DEVICE_STATE_ACTIVE
                    | DEVICE_STATE_DISABLED
                    | DEVICE_STATE_UNPLUGGED;

    ComPtr<IMMDeviceCollection> pCollection;
    HRESULT hr = m_impl->pEnumerator->EnumAudioEndpoints(
        flow, stateMask, &pCollection);
    if (FAILED(hr)) {
        return Result<std::vector<DeviceInfo>, VarError>::err(
            VarError::fromHresult(ErrorCode::WasapiError,
                static_cast<uint32_t>(hr), "EnumAudioEndpoints"));
    }

    UINT count = 0;
    pCollection->GetCount(&count);

    // Get the default device ID for comparison
    std::string defaultId;
    {
        ComPtr<IMMDevice> pDefault;
        if (SUCCEEDED(m_impl->pEnumerator->GetDefaultAudioEndpoint(
                flow, eMultimedia, &pDefault))) {
            LPWSTR pwszId = nullptr;
            if (SUCCEEDED(pDefault->GetId(&pwszId)) && pwszId) {
                defaultId = WideToUtf8(pwszId);
                CoTaskMemFree(pwszId);
            }
        }
    }

    std::vector<DeviceInfo> devices;
    devices.reserve(count);

    for (UINT i = 0; i < count; ++i) {
        ComPtr<IMMDevice> pDevice;
        if (FAILED(pCollection->Item(i, &pDevice))) continue;

        // Check device ID to determine if it is the default
        std::string thisId;
        {
            LPWSTR pwszId = nullptr;
            if (SUCCEEDED(pDevice->GetId(&pwszId)) && pwszId) {
                thisId = WideToUtf8(pwszId);
                CoTaskMemFree(pwszId);
            }
        }
        bool isDefault = (!defaultId.empty() && thisId == defaultId);

        DeviceInfo info = DeviceInfoFromIMMDevice(pDevice.Get(), isDefault, m_logger);
        devices.push_back(std::move(info));
    }

    m_logger.Info("DeviceManager: Enumerated " +
                  std::to_string(devices.size()) + " device(s).");

    return Result<std::vector<DeviceInfo>, VarError>::ok(std::move(devices));
}

Result<DeviceInfo, VarError>
DeviceManager::GetDeviceById(const std::string& deviceId) {

    if (!m_initialized || !m_impl->pEnumerator) {
        return Result<DeviceInfo, VarError>::err(
            VarError{ ErrorCode::EngineNotInitialized });
    }

    // Convert device ID to wide string
    int wlen = MultiByteToWideChar(CP_UTF8, 0,
                                   deviceId.c_str(), -1, nullptr, 0);
    std::wstring wideId(wlen - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, deviceId.c_str(), -1,
                        wideId.data(), wlen);

    ComPtr<IMMDevice> pDevice;
    HRESULT hr = m_impl->pEnumerator->GetDevice(wideId.c_str(), &pDevice);
    if (FAILED(hr)) {
        return Result<DeviceInfo, VarError>::err(
            VarError::fromHresult(ErrorCode::DeviceNotFound,
                static_cast<uint32_t>(hr),
                "GetDevice('" + deviceId + "')"));
    }

    // Determine if default
    std::string defaultId;
    {
        ComPtr<IMMDevice> pDefault;
        if (SUCCEEDED(m_impl->pEnumerator->GetDefaultAudioEndpoint(
                eRender, eMultimedia, &pDefault))) {
            LPWSTR pwszId = nullptr;
            if (SUCCEEDED(pDefault->GetId(&pwszId)) && pwszId) {
                defaultId = WideToUtf8(pwszId);
                CoTaskMemFree(pwszId);
            }
        }
    }

    DeviceInfo info = DeviceInfoFromIMMDevice(pDevice.Get(),
                                              deviceId == defaultId,
                                              m_logger);
    return Result<DeviceInfo, VarError>::ok(std::move(info));
}

Result<DeviceInfo, VarError>
DeviceManager::GetDefaultDevice(DeviceType type) {

    if (!m_initialized || !m_impl->pEnumerator) {
        return Result<DeviceInfo, VarError>::err(
            VarError{ ErrorCode::EngineNotInitialized });
    }

    EDataFlow flow = (type == DeviceType::Render) ? eRender : eCapture;

    ComPtr<IMMDevice> pDevice;
    HRESULT hr = m_impl->pEnumerator->GetDefaultAudioEndpoint(
        flow, eMultimedia, &pDevice);
    if (FAILED(hr)) {
        return Result<DeviceInfo, VarError>::err(
            VarError::fromHresult(ErrorCode::DeviceNotFound,
                static_cast<uint32_t>(hr),
                "GetDefaultAudioEndpoint"));
    }

    DeviceInfo info = DeviceInfoFromIMMDevice(pDevice.Get(),
                                              /*isDefault=*/true,
                                              m_logger);
    m_logger.Info("DeviceManager: Default device = '" + info.name + "'");
    return Result<DeviceInfo, VarError>::ok(std::move(info));
}

// ---------------------------------------------------------------------------
// Monitoring (Phase 3: plug/unplug events — stub for now)
// ---------------------------------------------------------------------------

VoidResult DeviceManager::StartMonitoring() {
    if (!m_initialized || !m_impl->pEnumerator) {
        return VoidResult::err(VarError{ ErrorCode::EngineNotInitialized });
    }
    if (m_impl->pClient) return VoidResult::ok();

    m_impl->pClient = new NotificationClient(m_impl->callback);
    HRESULT hr = m_impl->pEnumerator->RegisterEndpointNotificationCallback(m_impl->pClient);
    if (FAILED(hr)) {
        m_impl->pClient->Release();
        m_impl->pClient = nullptr;
        return VoidResult::err(VarError::fromHresult(ErrorCode::WasapiError,
                                                    static_cast<uint32_t>(hr),
                                                    "RegisterEndpointNotificationCallback"));
    }

    m_logger.Info("DeviceManager: Monitoring started.");
    return VoidResult::ok();
}

void DeviceManager::StopMonitoring() {
    if (m_impl->pClient && m_impl->pEnumerator) {
        m_impl->pEnumerator->UnregisterEndpointNotificationCallback(m_impl->pClient);
        m_impl->pClient->Release();
        m_impl->pClient = nullptr;
        m_logger.Info("DeviceManager: Monitoring stopped.");
    }
}

} // namespace var
