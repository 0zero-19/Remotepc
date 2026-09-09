#pragma once
// =============================================================================
// ClassroomMonitor — Video Encoder
//
// H.264 кодирование через Windows Media Foundation.
// Принимает сырые BGRA кадры, выдаёт H.264 NAL units.
// =============================================================================

#include "common/VideoTypes.h"

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mftransform.h>
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

    /// Инициализация Media Foundation и H.264 кодека
    bool initialize(const video::EncoderConfig& config);

    /// Закодировать один сырой кадр
    /// @param rawFrame — входной кадр (BGRA пиксели)
    /// @param outEncoded — выходной закодированный кадр (H.264)
    /// @return true если кадр успешно закодирован
    bool encodeFrame(const video::RawFrame& rawFrame, video::EncodedFrame& outEncoded);

    /// Обновить параметры кодирования (качество, FPS)
    bool updateConfig(const video::EncoderConfig& config);

    /// Освободить ресурсы
    void shutdown();

private:
    bool createEncoder();
    bool configureEncoder();

    ComPtr<IMFTransform>    m_encoder;
    ComPtr<IMFMediaType>    m_inputType;
    ComPtr<IMFMediaType>    m_outputType;

    video::EncoderConfig    m_config;
    uint32_t                m_frameCount = 0;
    bool                    m_initialized = false;
};

} // namespace client
} // namespace cm
