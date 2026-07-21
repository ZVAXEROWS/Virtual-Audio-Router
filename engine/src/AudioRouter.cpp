// =============================================================================
// AudioRouter.cpp — Virtual Audio Router
// =============================================================================
// Phase 3: Loopback capture + Multi-device fan-out implemented.
// =============================================================================

#include <windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>

#include "var/AudioRouter.h"
#include "var/IDevice.h"
#include "var/Logger.h"
#include "var/BufferManager.h"
#include "var/LatencyManager.h"

#include <mutex>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>

using Microsoft::WRL::ComPtr;

namespace var {

// ---------------------------------------------------------------------------
// Impl
// ---------------------------------------------------------------------------

struct AudioRouter::Impl {
    std::vector<IDevice*>      outputs;
    mutable std::mutex         outputsMutex;

    ComPtr<IMMDevice>          pCaptureDevice;
    ComPtr<IAudioClient>       pAudioClient;
    ComPtr<IAudioCaptureClient> pCaptureClient;
    HANDLE                     hEvent { nullptr };
    bool                       comInitialized { false };

    std::thread                captureThread;
    std::atomic<bool>          captureRunning { false };
    AudioFormat                captureFormat;
    std::string                captureDeviceId;
    UINT32                     bufferFrameCount { 0 };
};

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

AudioRouter::AudioRouter(Logger& logger, BufferManager& buffer, LatencyManager& latency)
    : m_impl(std::make_unique<Impl>())
    , m_logger(logger)
    , m_buffer(buffer)
    , m_latency(latency)
{
    m_impl->hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
}

AudioRouter::~AudioRouter() {
    Shutdown();
    if (m_impl->hEvent) CloseHandle(m_impl->hEvent);
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

VoidResult AudioRouter::Initialize() {
    m_logger.Info("AudioRouter: Initialized.");
    return VoidResult::ok();
}

void AudioRouter::Shutdown() {
    if (m_impl->captureRunning.load()) {
        m_routing.store(false);
        m_impl->captureRunning.store(false);
        if (m_impl->captureThread.joinable()) {
            m_impl->captureThread.join();
        }
    }

    std::lock_guard lock(m_impl->outputsMutex);
    m_impl->outputs.clear();

    m_impl->pCaptureClient.Reset();
    if (m_impl->pAudioClient) {
        m_impl->pAudioClient->Stop();
        m_impl->pAudioClient.Reset();
    }
    m_impl->pCaptureDevice.Reset();

    m_logger.Info("AudioRouter: Shutdown complete.");
}

VoidResult AudioRouter::ConfigureOutputs(std::vector<IDevice*> devices) {
    std::lock_guard lock(m_impl->outputsMutex);
    m_impl->outputs = std::move(devices);
    m_logger.Info("AudioRouter: Configured with " +
                  std::to_string(m_impl->outputs.size()) + " output(s).");
    return VoidResult::ok();
}

void AudioRouter::RemoveDevice(const std::string& deviceId) {
    std::lock_guard lock(m_impl->outputsMutex);
    auto& v = m_impl->outputs;
    v.erase(std::remove_if(v.begin(), v.end(),
        [&deviceId](IDevice* d) {
            return d->GetDeviceInfo().id == deviceId;
        }), v.end());
    m_logger.Info("AudioRouter: Removed device " + deviceId);
}

// ---------------------------------------------------------------------------
// Loopback Capture & Routing
// ---------------------------------------------------------------------------



VoidResult AudioRouter::RouteBuffer(uint32_t /*frameCount*/) {
    // This is called by AudioEngine::StartRouting to actually kick off the capture loop.
    if (m_impl->captureRunning.load()) {
        return VoidResult::err(VarError{ErrorCode::AlreadyInitialized, "Capture already running."});
    }

    m_logger.Info("AudioRouter: Initializing loopback capture...");

    // 1. Get default render device (we loopback from what the user hears)
    ComPtr<IMMDeviceEnumerator> pEnumerator;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                  __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    if (FAILED(hr)) return VoidResult::err(VarError::fromHresult(ErrorCode::ComInitFailed, hr, "CoCreateInstance"));

    hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &m_impl->pCaptureDevice);
    if (FAILED(hr)) return VoidResult::err(VarError::fromHresult(ErrorCode::DeviceNotFound, hr, "GetDefaultAudioEndpoint"));

    LPWSTR pwszId = nullptr;
    if (SUCCEEDED(m_impl->pCaptureDevice->GetId(&pwszId)) && pwszId) {
        int needed = WideCharToMultiByte(CP_UTF8, 0, pwszId, -1, nullptr, 0, nullptr, nullptr);
        if (needed > 0) {
            std::string result(needed - 1, '\0');
            WideCharToMultiByte(CP_UTF8, 0, pwszId, -1, result.data(), needed, nullptr, nullptr);
            m_impl->captureDeviceId = result;
        }
        CoTaskMemFree(pwszId);
    }

    // 2. Activate Audio Client
    hr = m_impl->pCaptureDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&m_impl->pAudioClient);
    if (FAILED(hr)) return VoidResult::err(VarError::fromHresult(ErrorCode::WasapiError, hr, "Activate IAudioClient"));

    // 3. Get Mix Format
    WAVEFORMATEX* pFormat = nullptr;
    hr = m_impl->pAudioClient->GetMixFormat(&pFormat);
    if (FAILED(hr)) return VoidResult::err(VarError::fromHresult(ErrorCode::WasapiError, hr, "GetMixFormat"));

    m_impl->captureFormat.sampleRate = pFormat->nSamplesPerSec;
    m_impl->captureFormat.channels = pFormat->nChannels;
    m_impl->captureFormat.bitsPerSample = pFormat->wBitsPerSample;
    m_impl->captureFormat.isFloat = (pFormat->wFormatTag == WAVE_FORMAT_IEEE_FLOAT);
    if (pFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        auto* pExt = reinterpret_cast<WAVEFORMATEXTENSIBLE*>(pFormat);
        m_impl->captureFormat.isFloat = (pExt->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT);
        m_impl->captureFormat.bitsPerSample = pExt->Samples.wValidBitsPerSample;
    }

    m_logger.Info("AudioRouter: Loopback format is " + std::to_string(m_impl->captureFormat.sampleRate) + "Hz, " +
                  std::to_string(m_impl->captureFormat.channels) + " channels, Float=" + std::to_string(m_impl->captureFormat.isFloat));

    // 4. Initialize for Loopback + Event Driven
    hr = m_impl->pAudioClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
        0, 0, pFormat, nullptr
    );
    if (FAILED(hr)) {
        CoTaskMemFree(pFormat);
        return VoidResult::err(VarError::fromHresult(ErrorCode::WasapiError, hr, "IAudioClient::Initialize loopback"));
    }

    hr = m_impl->pAudioClient->SetEventHandle(m_impl->hEvent);
    if (FAILED(hr)) {
        CoTaskMemFree(pFormat);
        return VoidResult::err(VarError::fromHresult(ErrorCode::WasapiError, hr, "SetEventHandle"));
    }

    hr = m_impl->pAudioClient->GetBufferSize(&m_impl->bufferFrameCount);
    hr = m_impl->pAudioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&m_impl->pCaptureClient);
    if (FAILED(hr)) {
        CoTaskMemFree(pFormat);
        return VoidResult::err(VarError::fromHresult(ErrorCode::WasapiError, hr, "GetService(IAudioCaptureClient)"));
    }

    // Initialize all outputs with the capture format
    {
        std::lock_guard lock(m_impl->outputsMutex);
        for (auto* device : m_impl->outputs) {
            auto initRes = device->Initialize(m_impl->captureFormat);
            if (initRes) {
                device->Start();
            } else {
                m_logger.Error("AudioRouter: Failed to init output device: " + initRes.error().message);
            }
        }
    }

    // Start Capture
    hr = m_impl->pAudioClient->Start();
    if (FAILED(hr)) {
        CoTaskMemFree(pFormat);
        return VoidResult::err(VarError::fromHresult(ErrorCode::WasapiError, hr, "IAudioClient::Start (Capture)"));
    }
    
    CoTaskMemFree(pFormat);

    m_routing.store(true);
    m_impl->captureRunning.store(true);
    
    // Capture thread lambda
    auto captureFunc = [this]() {
        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        m_logger.Info("AudioRouter: Capture thread started.");

        while (m_impl->captureRunning.load()) {
            DWORD waitResult = WaitForSingleObject(m_impl->hEvent, 100);
            if (waitResult != WAIT_OBJECT_0) continue;

            UINT32 packetLength = 0;
            hr = m_impl->pCaptureClient->GetNextPacketSize(&packetLength);
            if (FAILED(hr)) continue;

            while (packetLength != 0) {
                BYTE* pData = nullptr;
                UINT32 numFramesAvailable = 0;
                DWORD flags = 0;

                hr = m_impl->pCaptureClient->GetBuffer(&pData, &numFramesAvailable, &flags, nullptr, nullptr);
                if (FAILED(hr)) break;

                if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                    static std::vector<BYTE> silenceBuf(192000, 0);
                    pData = silenceBuf.data();
                }

                if (numFramesAvailable > 0 && pData != nullptr) {
                    std::lock_guard lock(m_impl->outputsMutex);
                    for (auto* device : m_impl->outputs) {
                        if (device->GetDeviceInfo().id != m_impl->captureDeviceId) {
                            device->Write(reinterpret_cast<const float*>(pData), numFramesAvailable);
                        }
                    }
                }

                hr = m_impl->pCaptureClient->ReleaseBuffer(numFramesAvailable);
                if (FAILED(hr)) break;

                hr = m_impl->pCaptureClient->GetNextPacketSize(&packetLength);
                if (FAILED(hr)) break;
            }
        }

        m_logger.Info("AudioRouter: Capture thread stopped.");
        CoUninitialize();
    };
    
    m_impl->captureThread = std::thread(captureFunc);

    return VoidResult::ok();
}

uint32_t AudioRouter::OutputDeviceCount() const {
    std::lock_guard lock(m_impl->outputsMutex);
    return static_cast<uint32_t>(m_impl->outputs.size());
}

} // namespace var
