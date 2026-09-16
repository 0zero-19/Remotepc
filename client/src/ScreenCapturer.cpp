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

    m_useGdiFallback = false;

    // Пытаемся инициализировать DXGI Desktop Duplication
    bool dxgiOk = initD3D11() && initDuplication(outputIndex);

    if (dxgiOk) {
        m_initialized = true;
        std::cout << "[ScreenCapturer] Захват экрана: DXGI Desktop Duplication ("
                  << m_width << "x" << m_height << ")" << std::endl;
        return true;
    }

    // Если DXGI недоступен (гибридная графика / права / headless) — используем GDI BitBlt
    std::cout << "[ScreenCapturer] DXGI недоступен, включен резервный режим GDI BitBlt" << std::endl;
    m_width  = static_cast<uint32_t>(GetSystemMetrics(SM_CXSCREEN));
    m_height = static_cast<uint32_t>(GetSystemMetrics(SM_CYSCREEN));
    if (m_width == 0) m_width = 1920;
    if (m_height == 0) m_height = 1080;

    m_useGdiFallback = true;
    m_initialized = true;
    std::cout << "[ScreenCapturer] Захват экрана: GDI (" << m_width << "x" << m_height << ")" << std::endl;
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
    if (!m_initialized) return false;

    if (m_useGdiFallback) {
        return captureFrameGDI(outFrame);
    }

    bool success = captureFrameDXGI(outFrame);
    if (!success && !m_duplication) {
        // Если DXGI потерял соединение — переходим на GDI
        return captureFrameGDI(outFrame);
    }
    return success;
}

bool ScreenCapturer::captureFrameDXGI(video::RawFrame& outFrame) {
    if (!m_duplication) return false;

    // Получаем следующий кадр (таймаут 100 мс)
    ComPtr<IDXGIResource> desktopResource;
    DXGI_OUTDUPL_FRAME_INFO frameInfo;

    HRESULT hr = m_duplication->AcquireNextFrame(100, &frameInfo, desktopResource.GetAddressOf());

    if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
        return false;  // Нет изменений на экране
    }

    if (FAILED(hr)) {
        if (hr == DXGI_ERROR_ACCESS_LOST) {
            std::cout << "[ScreenCapturer] DXGI доступ потерян, переключение на GDI" << std::endl;
            shutdown();
            m_useGdiFallback = true;
            m_initialized = true;
            m_width  = static_cast<uint32_t>(GetSystemMetrics(SM_CXSCREEN));
            m_height = static_cast<uint32_t>(GetSystemMetrics(SM_CYSCREEN));
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

    // Нормализуем stride: убираем GPU padding (RowPitch может быть > width*4)
    const uint32_t expectedStride = m_width * 4;
    outFrame.stride = expectedStride;
    outFrame.pixels.resize(static_cast<size_t>(expectedStride) * m_height);

    const uint8_t* src = static_cast<const uint8_t*>(mapped.pData);
    uint8_t* dst = outFrame.pixels.data();

    if (mapped.RowPitch == expectedStride) {
        std::memcpy(dst, src, static_cast<size_t>(expectedStride) * m_height);
    } else {
        for (uint32_t row = 0; row < m_height; ++row) {
            std::memcpy(dst + row * expectedStride,
                        src + row * mapped.RowPitch,
                        expectedStride);
        }
    }

    // Освобождаем ресурсы
    m_context->Unmap(m_stagingTexture.Get(), 0);
    m_duplication->ReleaseFrame();

    return true;
}

bool ScreenCapturer::captureFrameGDI(video::RawFrame& outFrame) {
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    if (screenW <= 0 || screenH <= 0) return false;

    m_width  = static_cast<uint32_t>(screenW);
    m_height = static_cast<uint32_t>(screenH);

    HDC hScreenDC = GetDC(nullptr);
    if (!hScreenDC) return false;

    HDC hMemDC = CreateCompatibleDC(hScreenDC);
    if (!hMemDC) {
        ReleaseDC(nullptr, hScreenDC);
        return false;
    }

    BITMAPINFOHEADER bi = {};
    bi.biSize        = sizeof(BITMAPINFOHEADER);
    bi.biWidth       = screenW;
    bi.biHeight      = -screenH; // Top-down
    bi.biPlanes      = 1;
    bi.biBitCount    = 32;
    bi.biCompression = BI_RGB;

    void* pBits = nullptr;
    HBITMAP hBitmap = CreateDIBSection(hMemDC, reinterpret_cast<const BITMAPINFO*>(&bi),
                                       DIB_RGB_COLORS, &pBits, nullptr, 0);
    if (!hBitmap || !pBits) {
        DeleteDC(hMemDC);
        ReleaseDC(nullptr, hScreenDC);
        return false;
    }

    HBITMAP hOldBitmap = static_cast<HBITMAP>(SelectObject(hMemDC, hBitmap));
    BitBlt(hMemDC, 0, 0, screenW, screenH, hScreenDC, 0, 0, SRCCOPY | CAPTUREBLT);

    auto now = std::chrono::high_resolution_clock::now();
    outFrame.timestamp = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            now.time_since_epoch()).count());
    outFrame.width  = m_width;
    outFrame.height = m_height;
    outFrame.stride = m_width * 4;

    size_t totalBytes = static_cast<size_t>(outFrame.stride) * m_height;
    outFrame.pixels.resize(totalBytes);
    std::memcpy(outFrame.pixels.data(), pBits, totalBytes);

    SelectObject(hMemDC, hOldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(hMemDC);
    ReleaseDC(nullptr, hScreenDC);

    return true;
}

void ScreenCapturer::shutdown() {
    m_duplication.Reset();
    m_stagingTexture.Reset();
    m_context.Reset();
    m_device.Reset();
    m_initialized = false;
    m_useGdiFallback = false;
}

} // namespace client
} // namespace cm

