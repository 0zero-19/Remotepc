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
    // RPC_E_CHANGED_MODE (0x80010106) — COM уже инициализирован, это нормально

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

    // Качество JPEG: 50% для чёткого и быстрого стриминга
    m_compressionQuality = 0.50f;
    m_frameCount = 0;
    m_initialized = true;

    std::cout << "[VideoEncoder] Инициализация успешна (WIC JPEG, качество "
              << static_cast<int>(m_compressionQuality * 100) << "%)" << std::endl;

    return true;
}

bool VideoEncoder::encodeFrame(const video::RawFrame& rawFrame, video::EncodedFrame& outEncoded) {
    if (!m_initialized || !m_wicFactory) return false;
    if (rawFrame.pixels.empty() || rawFrame.width == 0 || rawFrame.height == 0) return false;

    // --- Шаг 1: Конвертируем BGRA → BGR (переиспользуем буфер без постоянных аллокаций) ---
    const uint32_t bgrStride = rawFrame.width * 3;
    // Выравниваем stride до 4 байт (требование WIC/GDI)
    const uint32_t bgrStridePadded = (bgrStride + 3) & ~3u;
    const size_t bgrSize = static_cast<size_t>(bgrStridePadded) * rawFrame.height;

    if (m_bgrBuffer.size() < bgrSize) {
        m_bgrBuffer.resize(bgrSize);
    }
    uint8_t* bgrPixels = m_bgrBuffer.data();

    const uint32_t srcStride = rawFrame.stride;  // width * 4, уже нормализован

    for (uint32_t y = 0; y < rawFrame.height; ++y) {
        const uint8_t* src = rawFrame.pixels.data() + y * srcStride;
        uint8_t* dst = bgrPixels + y * bgrStridePadded;
        for (uint32_t x = 0; x < rawFrame.width; ++x) {
            dst[x * 3 + 0] = src[x * 4 + 0]; // B
            dst[x * 3 + 1] = src[x * 4 + 1]; // G
            dst[x * 3 + 2] = src[x * 4 + 2]; // R
            // src[x * 4 + 3] = A — пропускаем
        }
    }

    // --- Шаг 2: Создаём поток памяти для JPEG ---
    IStream* pStream = nullptr;
    HRESULT hr = CreateStreamOnHGlobal(nullptr, TRUE, &pStream);
    if (FAILED(hr)) {
        std::cerr << "[VideoEncoder] CreateStreamOnHGlobal failed: 0x"
                  << std::hex << hr << std::dec << std::endl;
        return false;
    }

    ComPtr<IStream> spStream;
    spStream.Attach(pStream);

    // --- Шаг 3: Создаём JPEG кодировщик ---
    ComPtr<IWICBitmapEncoder> pEncoder;
    hr = m_wicFactory->CreateEncoder(GUID_ContainerFormatJpeg, nullptr, pEncoder.GetAddressOf());
    if (FAILED(hr)) {
        std::cerr << "[VideoEncoder] CreateEncoder(JPEG) failed: 0x"
                  << std::hex << hr << std::dec << std::endl;
        return false;
    }

    hr = pEncoder->Initialize(spStream.Get(), WICBitmapEncoderNoCache);
    if (FAILED(hr)) {
        std::cerr << "[VideoEncoder] pEncoder->Initialize failed: 0x"
                  << std::hex << hr << std::dec << std::endl;
        return false;
    }

    // --- Шаг 4: Создаём кадр с настройкой качества ---
    ComPtr<IWICBitmapFrameEncode> pFrameEncode;
    ComPtr<IPropertyBag2> pPropertyBag;
    hr = pEncoder->CreateNewFrame(pFrameEncode.GetAddressOf(), pPropertyBag.GetAddressOf());
    if (FAILED(hr)) {
        std::cerr << "[VideoEncoder] CreateNewFrame failed: 0x"
                  << std::hex << hr << std::dec << std::endl;
        return false;
    }

    PROPBAG2 option = { 0 };
    option.pstrName = const_cast<LPOLESTR>(L"ImageQuality");
    VARIANT varValue;
    VariantInit(&varValue);
    varValue.vt = VT_R4;
    varValue.fltVal = m_compressionQuality;
    pPropertyBag->Write(1, &option, &varValue);

    hr = pFrameEncode->Initialize(pPropertyBag.Get());
    if (FAILED(hr)) {
        std::cerr << "[VideoEncoder] pFrameEncode->Initialize failed: 0x"
                  << std::hex << hr << std::dec << std::endl;
        return false;
    }

    hr = pFrameEncode->SetSize(rawFrame.width, rawFrame.height);
    if (FAILED(hr)) {
        std::cerr << "[VideoEncoder] SetSize failed: 0x"
                  << std::hex << hr << std::dec << std::endl;
        return false;
    }

    // Устанавливаем 24bpp BGR формат — нативный для JPEG
    WICPixelFormatGUID format = GUID_WICPixelFormat24bppBGR;
    hr = pFrameEncode->SetPixelFormat(&format);
    if (FAILED(hr)) {
        std::cerr << "[VideoEncoder] SetPixelFormat failed: 0x"
                  << std::hex << hr << std::dec << std::endl;
        return false;
    }

    // --- Шаг 5: Записываем BGR пиксели ---
    hr = pFrameEncode->WritePixels(
        rawFrame.height,
        bgrStridePadded,
        static_cast<UINT>(bgrSize),
        bgrPixels
    );
    if (FAILED(hr)) {
        std::cerr << "[VideoEncoder] WritePixels failed: 0x"
                  << std::hex << hr << std::dec << " (stride="
                  << bgrStridePadded << " size=" << bgrSize << ")" << std::endl;
        return false;
    }

    hr = pFrameEncode->Commit();
    if (FAILED(hr)) {
        std::cerr << "[VideoEncoder] FrameEncode Commit failed: 0x"
                  << std::hex << hr << std::dec << std::endl;
        return false;
    }

    hr = pEncoder->Commit();
    if (FAILED(hr)) {
        std::cerr << "[VideoEncoder] Encoder Commit failed: 0x"
                  << std::hex << hr << std::dec << std::endl;
        return false;
    }

    // --- Шаг 6: Извлекаем JPEG байты ---
    HGLOBAL hGlobal = nullptr;
    hr = GetHGlobalFromStream(spStream.Get(), &hGlobal);
    if (FAILED(hr) || !hGlobal) {
        std::cerr << "[VideoEncoder] GetHGlobalFromStream failed" << std::endl;
        return false;
    }

    STATSTG stat = {};
    SIZE_T streamSize = 0;
    if (SUCCEEDED(spStream->Stat(&stat, STATFLAG_NONAME))) {
        streamSize = static_cast<SIZE_T>(stat.cbSize.QuadPart);
    } else {
        streamSize = GlobalSize(hGlobal);
    }

    if (streamSize == 0) {
        std::cerr << "[VideoEncoder] Encoded JPEG size is 0" << std::endl;
        return false;
    }

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

    // Логируем первый кадр для диагностики
    if (m_frameCount == 0) {
        std::cout << "[VideoEncoder] Первый кадр закодирован: " << rawFrame.width << "x" << rawFrame.height
                  << " BGRA stride=" << rawFrame.stride
                  << " -> JPEG " << streamSize << " байт" << std::endl;
    }

    m_frameCount++;
    return true;
}

bool VideoEncoder::updateConfig(const video::EncoderConfig& config) {
    m_config = config;
    return true;
}

void VideoEncoder::shutdown() {
    m_bgrBuffer.clear();
    m_bgrBuffer.shrink_to_fit();
    m_wicFactory.Reset();
    m_initialized = false;
    if (m_comInitialized) {
        CoUninitialize();
        m_comInitialized = false;
    }
}

} // namespace client
} // namespace cm
