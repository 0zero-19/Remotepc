// =============================================================================
// ClassroomMonitor — Video Encoder Implementation
//
// Высокоскоростное сжатие кадров через Windows Imaging Component (WIC JPEG).
// Работает на любых видеокартах и редакциях Windows без сбоев.
// =============================================================================

#include "client/VideoEncoder.h"

#include <iostream>
#include <cstring>

#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

namespace cm {
namespace client {

VideoEncoder::VideoEncoder() = default;

VideoEncoder::~VideoEncoder() {
    shutdown();
}

bool VideoEncoder::initialize(const video::EncoderConfig& config) {
    m_config = config;

    // Инициализация COM для текущего потока (если еще не инициализирован)
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (SUCCEEDED(hr)) {
        m_comInitialized = true;
    }

    // Создаем WIC Imaging Factory
    hr = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(m_wicFactory.GetAddressOf())
    );

    if (FAILED(hr)) {
        // Пробуем альтернативный CLSID WIC Imaging Factory 1
        hr = CoCreateInstance(
            CLSID_WICImagingFactory1,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(m_wicFactory.GetAddressOf())
        );
    }

    if (FAILED(hr) || !m_wicFactory) {
        std::cerr << "[VideoEncoder] CoCreateInstance(WICImagingFactory) failed: 0x"
                  << std::hex << hr << std::dec << std::endl;
        return false;
    }

    // Базовое качество JPEG: 65% для оптимального баланса качества и скорости передачи
    m_compressionQuality = 0.65f;
    m_frameCount = 0;
    m_initialized = true;

    return true;
}

bool VideoEncoder::encodeFrame(const video::RawFrame& rawFrame, video::EncodedFrame& outEncoded) {
    if (!m_initialized || !m_wicFactory) return false;
    if (rawFrame.pixels.empty() || rawFrame.width == 0 || rawFrame.height == 0) return false;

    // 1. Создаем WIC Bitmap из сырых BGRA пикселей
    ComPtr<IWICBitmap> pBitmap;
    HRESULT hr = m_wicFactory->CreateBitmapFromMemory(
        rawFrame.width,
        rawFrame.height,
        GUID_WICPixelFormat32bppBGRA,
        static_cast<UINT>(rawFrame.stride),
        static_cast<UINT>(rawFrame.pixels.size()),
        const_cast<BYTE*>(rawFrame.pixels.data()),
        pBitmap.GetAddressOf()
    );
    if (FAILED(hr)) return false;

    // 2. Конвертируем BGRA → 24bppBGR (JPEG не поддерживает альфа-канал!)
    ComPtr<IWICFormatConverter> pConverter;
    hr = m_wicFactory->CreateFormatConverter(pConverter.GetAddressOf());
    if (FAILED(hr)) return false;

    hr = pConverter->Initialize(
        pBitmap.Get(),
        GUID_WICPixelFormat24bppBGR,
        WICBitmapDitherTypeNone,
        nullptr,
        0.0,
        WICBitmapPaletteTypeCustom
    );
    if (FAILED(hr)) return false;

    // 3. Создаем поток памяти для JPEG
    IStream* pStream = nullptr;
    hr = CreateStreamOnHGlobal(nullptr, TRUE, &pStream);
    if (FAILED(hr)) return false;

    ComPtr<IStream> spStream;
    spStream.Attach(pStream);

    // 4. Создаем JPEG энкодер
    ComPtr<IWICBitmapEncoder> pEncoder;
    hr = m_wicFactory->CreateEncoder(GUID_ContainerFormatJpeg, nullptr, pEncoder.GetAddressOf());
    if (FAILED(hr)) return false;

    hr = pEncoder->Initialize(spStream.Get(), WICBitmapEncoderNoCache);
    if (FAILED(hr)) return false;

    // 5. Создаем кадр с настройками качества
    ComPtr<IWICBitmapFrameEncode> pFrameEncode;
    ComPtr<IPropertyBag2> pPropertyBag;
    hr = pEncoder->CreateNewFrame(pFrameEncode.GetAddressOf(), pPropertyBag.GetAddressOf());
    if (FAILED(hr)) return false;

    PROPBAG2 option = { 0 };
    option.pstrName = const_cast<LPOLESTR>(L"ImageQuality");
    VARIANT varValue;
    VariantInit(&varValue);
    varValue.vt = VT_R4;
    varValue.fltVal = m_compressionQuality;
    pPropertyBag->Write(1, &option, &varValue);

    hr = pFrameEncode->Initialize(pPropertyBag.Get());
    if (FAILED(hr)) return false;

    hr = pFrameEncode->SetSize(rawFrame.width, rawFrame.height);
    if (FAILED(hr)) return false;

    WICPixelFormatGUID format = GUID_WICPixelFormat24bppBGR;
    hr = pFrameEncode->SetPixelFormat(&format);
    if (FAILED(hr)) return false;

    // 6. Записываем сконвертированные пиксели из IWICFormatConverter
    hr = pFrameEncode->WriteSource(pConverter.Get(), nullptr);
    if (FAILED(hr)) return false;

    hr = pFrameEncode->Commit();
    if (FAILED(hr)) return false;

    hr = pEncoder->Commit();
    if (FAILED(hr)) return false;

    // 7. Извлекаем готовые JPEG-байты из потока
    HGLOBAL hGlobal = nullptr;
    hr = GetHGlobalFromStream(spStream.Get(), &hGlobal);
    if (FAILED(hr) || !hGlobal) return false;

    STATSTG stat = {};
    SIZE_T streamSize = 0;
    if (SUCCEEDED(spStream->Stat(&stat, STATFLAG_NONAME))) {
        streamSize = static_cast<SIZE_T>(stat.cbSize.QuadPart);
    } else {
        streamSize = GlobalSize(hGlobal);
    }

    if (streamSize == 0) return false;

    void* pData = GlobalLock(hGlobal);
    if (!pData) return false;

    outEncoded.data.assign(
        reinterpret_cast<const uint8_t*>(pData),
        reinterpret_cast<const uint8_t*>(pData) + streamSize
    );
    GlobalUnlock(hGlobal);

    outEncoded.width     = rawFrame.width;
    outEncoded.height    = rawFrame.height;
    outEncoded.timestamp = rawFrame.timestamp;
    outEncoded.type      = video::FrameType::I_FRAME;

    m_frameCount++;
    return true;
}

bool VideoEncoder::updateConfig(const video::EncoderConfig& config) {
    m_config = config;
    return true;
}

void VideoEncoder::shutdown() {
    m_wicFactory.Reset();
    m_initialized = false;
    if (m_comInitialized) {
        CoUninitialize();
        m_comInitialized = false;
    }
}

} // namespace client
} // namespace cm
