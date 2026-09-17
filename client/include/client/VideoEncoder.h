#pragma once
// =============================================================================
// ClassroomMonitor — Video Encoder
//
// Высокоскоростное сжатие кадров (WIC JPEG) для передачи по сети.
// =============================================================================

#include "common/VideoTypes.h"

#include <wincodec.h>
#include <wrl/client.h>

namespace cm {
namespace client {

using Microsoft::WRL::ComPtr;

class VideoEncoder {
public:
    VideoEncoder();
    ~VideoEncoder();

    VideoEncoder(const VideoEncoder&) = delete;
    VideoEncoder& operator=(const VideoEncoder&) = delete;

    /// Инициализация кодировщика
    bool initialize(const video::EncoderConfig& config);

    /// Закодировать один сырой кадр (BGRA) в JPEG
    bool encodeFrame(const video::RawFrame& rawFrame, video::EncodedFrame& outEncoded);

    /// Обновить параметры кодирования
    bool updateConfig(const video::EncoderConfig& config);

    /// Освободить ресурсы
    void shutdown();

private:
    ComPtr<IWICImagingFactory> m_wicFactory;
    video::EncoderConfig       m_config;
    uint32_t                   m_frameCount = 0;
    bool                       m_initialized = false;
    float                      m_compressionQuality = 0.50f;
    bool                       m_comInitialized = false;
    std::vector<uint8_t>       m_bgrBuffer;
};

} // namespace client
} // namespace cm
