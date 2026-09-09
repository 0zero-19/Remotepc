// =============================================================================
// ClassroomMonitor — Screen Capturer Implementation
//
// DXGI Desktop Duplication API
// =============================================================================

#include "client/ScreenCapturer.h"

#include <chrono>
#include <iostream>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

namespace cm {
namespace client {

ScreenCapturer::ScreenCapturer() = default;

ScreenCapturer::~ScreenCapturer() {
    shutdown();
}

bool ScreenCapturer::initialize(uint32_t outputIndex) {
    if (m_initialized) {
        shutdown();
    }

    if (!initD3D11()) {
        std::cerr << "[ScreenCapturer] Failed to init D3D11" << std::endl;
        return false;
    }

    if (!initDuplication(outputIndex)) {
        std::cerr << "[ScreenCapturer] Failed to init Desktop Duplication" << std::endl;
        return false;
    }

    m_initialized = true;
    return true;
}

bool ScreenCapturer::initD3D11() {
    D3D_FEATURE_LEVEL featureLevel;
    D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0 };

    HRESULT hr = D3D11CreateDevice(
        nullptr,                    // Адаптер по умолчанию
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,                    // Без программного растеризатора
        0,                          // Флаги
        featureLevels,
        ARRAYSIZE(featureLevels),
        D3D11_SDK_VERSION,
        m_device.GetAddressOf(),
        &featureLevel,
        m_context.GetAddressOf()
    );

    return SUCCEEDED(hr);
}

bool ScreenCapturer::initDuplication(uint32_t outputIndex) {
    // Получаем DXGI Device
    ComPtr<IDXGIDevice> dxgiDevice;
    HRESULT hr = m_device.As(&dxgiDevice);
    if (FAILED(hr)) return false;

    // Получаем DXGI Adapter
    ComPtr<IDXGIAdapter> adapter;
    hr = dxgiDevice->GetAdapter(adapter.GetAddressOf());
    if (FAILED(hr)) return false;

    // Получаем DXGI Output (монитор)
    ComPtr<IDXGIOutput> output;
    hr = adapter->EnumOutputs(outputIndex, output.GetAddressOf());
    if (FAILED(hr)) return false;

    // Получаем описание выхода для размеров экрана
    DXGI_OUTPUT_DESC outputDesc;
    output->GetDesc(&outputDesc);
    m_width  = outputDesc.DesktopCoordinates.right - outputDesc.DesktopCoordinates.left;
    m_height = outputDesc.DesktopCoordinates.bottom - outputDesc.DesktopCoordinates.top;

    // Получаем IDXGIOutput1 для Desktop Duplication
    ComPtr<IDXGIOutput1> output1;
    hr = output.As(&output1);
    if (FAILED(hr)) return false;

    // Создаём дупликатор
    hr = output1->DuplicateOutput(m_device.Get(), m_duplication.GetAddressOf());
    if (FAILED(hr)) {
        std::cerr << "[ScreenCapturer] DuplicateOutput failed: 0x"
                  << std::hex << hr << std::dec << std::endl;
        return false;
    }

    // Создаём staging текстуру для копирования из GPU → CPU
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width              = m_width;
    texDesc.Height             = m_height;
    texDesc.MipLevels          = 1;
    texDesc.ArraySize          = 1;
    texDesc.Format             = DXGI_FORMAT_B8G8R8A8_UNORM;
    texDesc.SampleDesc.Count   = 1;
    texDesc.Usage              = D3D11_USAGE_STAGING;
    texDesc.CPUAccessFlags     = D3D11_CPU_ACCESS_READ;

    hr = m_device->CreateTexture2D(&texDesc, nullptr, m_stagingTexture.GetAddressOf());
    return SUCCEEDED(hr);
}

bool ScreenCapturer::captureFrame(video::RawFrame& outFrame) {
    if (!m_initialized || !m_duplication) {
        return false;
    }

    // Получаем следующий кадр (таймаут 100 мс)
    ComPtr<IDXGIResource> desktopResource;
    DXGI_OUTDUPL_FRAME_INFO frameInfo;

    HRESULT hr = m_duplication->AcquireNextFrame(100, &frameInfo, desktopResource.GetAddressOf());

    if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
        return false;  // Нет изменений на экране
    }

    if (FAILED(hr)) {
        // Возможно нужна реинициализация (смена разрешения, UAC и т.д.)
        if (hr == DXGI_ERROR_ACCESS_LOST) {
            shutdown();
            initialize();
        }
        return false;
    }

    // Получаем текстуру рабочего стола
    ComPtr<ID3D11Texture2D> desktopTexture;
    hr = desktopResource.As(&desktopTexture);
    if (FAILED(hr)) {
        m_duplication->ReleaseFrame();
        return false;
    }

    // Копируем в staging текстуру (GPU → CPU-доступная память)
    m_context->CopyResource(m_stagingTexture.Get(), desktopTexture.Get());

    // Маппим текстуру для чтения
    D3D11_MAPPED_SUBRESOURCE mapped;
    hr = m_context->Map(m_stagingTexture.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) {
        m_duplication->ReleaseFrame();
        return false;
    }

    // Заполняем выходной кадр
    auto now = std::chrono::high_resolution_clock::now();
    outFrame.timestamp = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            now.time_since_epoch()).count());
    outFrame.width  = m_width;
    outFrame.height = m_height;
    outFrame.stride = mapped.RowPitch;

    // Копируем пиксели
    const size_t dataSize = static_cast<size_t>(mapped.RowPitch) * m_height;
    outFrame.pixels.resize(dataSize);
    std::memcpy(outFrame.pixels.data(), mapped.pData, dataSize);

    // Освобождаем ресурсы
    m_context->Unmap(m_stagingTexture.Get(), 0);
    m_duplication->ReleaseFrame();

    return true;
}

void ScreenCapturer::shutdown() {
    m_duplication.Reset();
    m_stagingTexture.Reset();
    m_context.Reset();
    m_device.Reset();
    m_initialized = false;
}

} // namespace client
} // namespace cm
