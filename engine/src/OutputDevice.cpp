// =============================================================================
// OutputDevice.cpp — Virtual Audio Router
// =============================================================================
// Phase 3: Full WASAPI IAudioClient / IAudioRenderClient implementation.
//
// We use AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM so Windows automatically resamples
// our common mixing format (e.g., 48kHz from capture) to whatever the physical
// device requires (e.g., 44.1kHz headphones).
// =============================================================================

#include <windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>

#include "var/Logger.h"
#include "var/OutputDevice.h"
#include "var/Constants.h"

using Microsoft::WRL::ComPtr;

namespace var {

// ---------------------------------------------------------------------------
// Impl
// ---------------------------------------------------------------------------

struct OutputDevice::Impl {
    ComPtr<IMMDevice>          pDevice;
    ComPtr<IAudioClient>       pAudioClient;
    ComPtr<IAudioRenderClient> pRenderClient;
    HANDLE                     hEvent { nullptr };
    UINT32                     bufferFrameCount { 0 };
    bool                       comInitialized { false };
    bool                       initialized { false };
};

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

OutputDevice::OutputDevice(DeviceInfo info, Logger& logger)
    : m_impl(std::make_unique<Impl>())
    , m_info(std::move(info))
    , m_logger(logger)
{
    m_impl->hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
}

OutputDevice::~OutputDevice() {
    if (m_running.load()) Stop();
    Shutdown();
    if (m_impl->hEvent) CloseHandle(m_impl->hEvent);
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

VoidResult OutputDevice::Initialize(const AudioFormat& requestedFormat) {
    if (m_impl->initialized) {
        return VoidResult::err(VarError{ ErrorCode::AlreadyInitialized });
    }

    m_logger.Info("OutputDevice [" + m_info.name + "]: Initializing WASAPI render client...");
    m_actualFormat = requestedFormat;

    // Initialize COM for this thread if needed
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE) {
        m_impl->comInitialized = (hr != RPC_E_CHANGED_MODE);
    }

    // 1. Get the IMMDevice from the enumerator
    ComPtr<IMMDeviceEnumerator> pEnumerator;
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                          __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    if (FAILED(hr)) return VoidResult::err(VarError::fromHresult(ErrorCode::ComInitFailed, hr, "CoCreateInstance(IMMDeviceEnumerator)"));

    // Convert UTF-8 ID to wide string
    int wlen = MultiByteToWideChar(CP_UTF8, 0, m_info.id.c_str(), -1, nullptr, 0);
    std::wstring wideId(wlen - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, m_info.id.c_str(), -1, wideId.data(), wlen);

    hr = pEnumerator->GetDevice(wideId.c_str(), &m_impl->pDevice);
    if (FAILED(hr)) return VoidResult::err(VarError::fromHresult(ErrorCode::DeviceNotFound, hr, "GetDevice"));

    // 2. Activate IAudioClient
    hr = m_impl->pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&m_impl->pAudioClient);
    if (FAILED(hr)) return VoidResult::err(VarError::fromHresult(ErrorCode::WasapiError, hr, "Activate IAudioClient"));

    // 3. Prepare WAVEFORMATEX for requested format (usually 32-bit float from capture)
    WAVEFORMATEXTENSIBLE wfx = {};
    wfx.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    wfx.Format.nChannels = requestedFormat.channels;
    wfx.Format.nSamplesPerSec = requestedFormat.sampleRate;
    wfx.Format.wBitsPerSample = requestedFormat.bitsPerSample; // Should be 32 for float
    wfx.Format.nBlockAlign = (wfx.Format.nChannels * wfx.Format.wBitsPerSample) / 8;
    wfx.Format.nAvgBytesPerSec = wfx.Format.nSamplesPerSec * wfx.Format.nBlockAlign;
    wfx.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
    wfx.Samples.wValidBitsPerSample = wfx.Format.wBitsPerSample;
    wfx.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;

    // 4. Initialize IAudioClient with Auto-Convert
    // This allows us to feed 48kHz float to a 44.1kHz device and Windows handles the resampling
    DWORD streamFlags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK | 
                        AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | 
                        AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;

    REFERENCE_TIME bufferDuration = 200000; // 20ms in 100-ns units
    
    hr = m_impl->pAudioClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        streamFlags,
        bufferDuration,
        0,
        &wfx.Format,
        nullptr
    );

    if (FAILED(hr)) return VoidResult::err(VarError::fromHresult(ErrorCode::WasapiError, hr, "IAudioClient::Initialize"));

    // 5. Set Event Handle
    hr = m_impl->pAudioClient->SetEventHandle(m_impl->hEvent);
    if (FAILED(hr)) return VoidResult::err(VarError::fromHresult(ErrorCode::WasapiError, hr, "SetEventHandle"));

    // 6. Get Render Client
    hr = m_impl->pAudioClient->GetService(__uuidof(IAudioRenderClient), (void**)&m_impl->pRenderClient);
    if (FAILED(hr)) return VoidResult::err(VarError::fromHresult(ErrorCode::WasapiError, hr, "GetService(IAudioRenderClient)"));

    m_impl->pAudioClient->GetBufferSize(&m_impl->bufferFrameCount);
    
    m_impl->initialized = true;
    m_logger.Info("OutputDevice [" + m_info.name + "]: Initialized. Buffer=" + std::to_string(m_impl->bufferFrameCount) + " frames.");

    return VoidResult::ok();
}

VoidResult OutputDevice::Start() {
    if (!m_impl->initialized || !m_impl->pAudioClient) return VoidResult::err(VarError{ErrorCode::EngineNotInitialized});
    
    m_logger.Info("OutputDevice [" + m_info.name + "]: Starting playback.");
    HRESULT hr = m_impl->pAudioClient->Start();
    if (FAILED(hr)) return VoidResult::err(VarError::fromHresult(ErrorCode::WasapiError, hr, "IAudioClient::Start"));
    
    m_running.store(true);
    return VoidResult::ok();
}

VoidResult OutputDevice::Stop() {
    if (!m_running.load() || !m_impl->pAudioClient) return VoidResult::ok();
    
    m_logger.Info("OutputDevice [" + m_info.name + "]: Stopping playback.");
    m_impl->pAudioClient->Stop();
    m_running.store(false);
    
    return VoidResult::ok();
}

void OutputDevice::Shutdown() {
    if (!m_impl->initialized) return;
    m_logger.Info("OutputDevice [" + m_info.name + "]: Shutting down.");
    
    m_impl->pRenderClient.Reset();
    m_impl->pAudioClient.Reset();
    m_impl->pDevice.Reset();
    
    if (m_impl->comInitialized) {
        CoUninitialize();
        m_impl->comInitialized = false;
    }
    
    m_impl->initialized = false;
}

// ---------------------------------------------------------------------------
// Processing
// ---------------------------------------------------------------------------

VoidResult OutputDevice::Write(const float* data, uint32_t frameCount) {
    if (!m_running.load() || !m_impl->pRenderClient) return VoidResult::ok();

    UINT32 padding = 0;
    HRESULT hr = m_impl->pAudioClient->GetCurrentPadding(&padding);
    if (FAILED(hr)) return VoidResult::err(VarError::fromHresult(ErrorCode::WasapiError, hr, "GetCurrentPadding"));

    UINT32 available = m_impl->bufferFrameCount - padding;
    if (available == 0) return VoidResult::ok(); // Buffer full

    UINT32 toWrite = (frameCount < available) ? frameCount : available;

    BYTE* pData = nullptr;
    hr = m_impl->pRenderClient->GetBuffer(toWrite, &pData);
    if (FAILED(hr)) return VoidResult::err(VarError::fromHresult(ErrorCode::WasapiError, hr, "GetBuffer"));

    uint32_t bytesToWrite = toWrite * m_actualFormat.channels * (m_actualFormat.bitsPerSample / 8);
    memcpy(pData, data, bytesToWrite);

    hr = m_impl->pRenderClient->ReleaseBuffer(toWrite, 0);
    if (FAILED(hr)) return VoidResult::err(VarError::fromHresult(ErrorCode::WasapiError, hr, "ReleaseBuffer"));

    m_position.fetch_add(toWrite);
    return VoidResult::ok();
}

} // namespace var
