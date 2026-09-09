// =============================================================================
// ClassroomMonitor — Video Encoder Implementation
//
// H.264 кодирование через Media Foundation Transform (MFT).
// =============================================================================

#include "client/VideoEncoder.h"

#include <mfapi.h>
#include <mferror.h>
#include <codecapi.h>
#include <iostream>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "mf.lib")

namespace cm {
namespace client {

VideoEncoder::VideoEncoder() = default;

VideoEncoder::~VideoEncoder() {
    shutdown();
}

bool VideoEncoder::initialize(const video::EncoderConfig& config) {
    m_config = config;

    // Инициализация Media Foundation
    HRESULT hr = MFStartup(MF_VERSION);
    if (FAILED(hr)) {
        std::cerr << "[VideoEncoder] MFStartup failed" << std::endl;
        return false;
    }

    if (!createEncoder()) {
        std::cerr << "[VideoEncoder] Failed to create encoder" << std::endl;
        return false;
    }

    if (!configureEncoder()) {
        std::cerr << "[VideoEncoder] Failed to configure encoder" << std::endl;
        return false;
    }

    m_initialized = true;
    m_frameCount = 0;
    return true;
}

bool VideoEncoder::createEncoder() {
    // Ищем H.264 аппаратный кодек
    MFT_REGISTER_TYPE_INFO outputType = { MFMediaType_Video, MFVideoFormat_H264 };

    IMFActivate** activates = nullptr;
    UINT32 count = 0;

    HRESULT hr = MFTEnumEx(
        MFT_CATEGORY_VIDEO_ENCODER,
        MFT_ENUM_FLAG_HARDWARE | MFT_ENUM_FLAG_SORTANDFILTER,
        nullptr,
        &outputType,
        &activates,
        &count
    );

    // Если аппаратный кодек не найден — ищем программный
    if (FAILED(hr) || count == 0) {
        hr = MFTEnumEx(
            MFT_CATEGORY_VIDEO_ENCODER,
            MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_SORTANDFILTER,
            nullptr,
            &outputType,
            &activates,
            &count
        );
    }

    if (FAILED(hr) || count == 0) {
        std::cerr << "[VideoEncoder] No H.264 encoder found" << std::endl;
        return false;
    }

    // Активируем первый найденный кодек
    hr = activates[0]->ActivateObject(IID_PPV_ARGS(m_encoder.GetAddressOf()));

    // Освобождаем массив активаторов
    for (UINT32 i = 0; i < count; ++i) {
        activates[i]->Release();
    }
    CoTaskMemFree(activates);

    return SUCCEEDED(hr);
}

bool VideoEncoder::configureEncoder() {
    if (!m_encoder) return false;

    // Настраиваем выходной тип (H.264)
    HRESULT hr = MFCreateMediaType(m_outputType.GetAddressOf());
    if (FAILED(hr)) return false;

    m_outputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    m_outputType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
    m_outputType->SetUINT32(MF_MT_AVG_BITRATE, m_config.bitrate);
    MFSetAttributeSize(m_outputType.Get(), MF_MT_FRAME_SIZE, m_config.width, m_config.height);
    MFSetAttributeRatio(m_outputType.Get(), MF_MT_FRAME_RATE, m_config.fps, 1);
    m_outputType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    m_outputType->SetUINT32(MF_MT_MPEG2_PROFILE, eAVEncH264VProfile_Base);

    hr = m_encoder->SetOutputType(0, m_outputType.Get(), 0);
    if (FAILED(hr)) {
        std::cerr << "[VideoEncoder] SetOutputType failed: 0x"
                  << std::hex << hr << std::dec << std::endl;
        return false;
    }

    // Настраиваем входной тип (NV12 — стандарт для MF кодеков)
    hr = MFCreateMediaType(m_inputType.GetAddressOf());
    if (FAILED(hr)) return false;

    m_inputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    m_inputType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
    MFSetAttributeSize(m_inputType.Get(), MF_MT_FRAME_SIZE, m_config.width, m_config.height);
    MFSetAttributeRatio(m_inputType.Get(), MF_MT_FRAME_RATE, m_config.fps, 1);
    m_inputType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);

    hr = m_encoder->SetInputType(0, m_inputType.Get(), 0);
    if (FAILED(hr)) {
        std::cerr << "[VideoEncoder] SetInputType failed: 0x"
                  << std::hex << hr << std::dec << std::endl;
        return false;
    }

    // Запускаем поток обработки
    hr = m_encoder->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
    if (FAILED(hr)) return false;

    hr = m_encoder->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);
    return SUCCEEDED(hr);
}

bool VideoEncoder::encodeFrame(const video::RawFrame& rawFrame, video::EncodedFrame& outEncoded) {
    if (!m_initialized || !m_encoder) return false;

    // Создаём входной буфер MF
    ComPtr<IMFMediaBuffer> inputBuffer;
    HRESULT hr = MFCreateMemoryBuffer(
        static_cast<DWORD>(rawFrame.pixels.size()), inputBuffer.GetAddressOf());
    if (FAILED(hr)) return false;

    // Копируем данные в буфер
    BYTE* bufferData = nullptr;
    hr = inputBuffer->Lock(&bufferData, nullptr, nullptr);
    if (FAILED(hr)) return false;

    std::memcpy(bufferData, rawFrame.pixels.data(), rawFrame.pixels.size());
    inputBuffer->Unlock();
    inputBuffer->SetCurrentLength(static_cast<DWORD>(rawFrame.pixels.size()));

    // Создаём сэмпл
    ComPtr<IMFSample> inputSample;
    hr = MFCreateSample(inputSample.GetAddressOf());
    if (FAILED(hr)) return false;

    inputSample->AddBuffer(inputBuffer.Get());

    // Устанавливаем время кадра
    LONGLONG duration = 10000000LL / m_config.fps;  // в 100-наносекундных единицах
    inputSample->SetSampleTime(static_cast<LONGLONG>(m_frameCount) * duration);
    inputSample->SetSampleDuration(duration);

    // Подаём кадр на вход кодека
    hr = m_encoder->ProcessInput(0, inputSample.Get(), 0);
    if (FAILED(hr)) return false;

    // Получаем закодированные данные
    MFT_OUTPUT_DATA_BUFFER outputData = {};
    MFT_OUTPUT_STREAM_INFO streamInfo;
    hr = m_encoder->GetOutputStreamInfo(0, &streamInfo);
    if (FAILED(hr)) return false;

    ComPtr<IMFMediaBuffer> outputBuffer;
    hr = MFCreateMemoryBuffer(streamInfo.cbSize, outputBuffer.GetAddressOf());
    if (FAILED(hr)) return false;

    ComPtr<IMFSample> outputSample;
    hr = MFCreateSample(outputSample.GetAddressOf());
    if (FAILED(hr)) return false;

    outputSample->AddBuffer(outputBuffer.Get());
    outputData.pSample = outputSample.Get();

    DWORD status = 0;
    hr = m_encoder->ProcessOutput(0, 1, &outputData, &status);

    if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
        m_frameCount++;
        return false;  // Кодек ещё не выдал кадр
    }

    if (FAILED(hr)) return false;

    // Читаем закодированные данные
    ComPtr<IMFMediaBuffer> resultBuffer;
    hr = outputData.pSample->ConvertToContiguousBuffer(resultBuffer.GetAddressOf());
    if (FAILED(hr)) return false;

    BYTE* encodedData = nullptr;
    DWORD encodedSize = 0;
    hr = resultBuffer->Lock(&encodedData, nullptr, &encodedSize);
    if (FAILED(hr)) return false;

    // Заполняем выходной кадр
    outEncoded.data.assign(encodedData, encodedData + encodedSize);
    outEncoded.width     = m_config.width;
    outEncoded.height    = m_config.height;
    outEncoded.timestamp = rawFrame.timestamp;
    outEncoded.type      = (m_frameCount % m_config.iFrameInterval == 0)
                            ? video::FrameType::I_FRAME
                            : video::FrameType::P_FRAME;

    resultBuffer->Unlock();
    m_frameCount++;

    return true;
}

bool VideoEncoder::updateConfig(const video::EncoderConfig& config) {
    // Пересоздаём кодек с новыми параметрами
    shutdown();
    return initialize(config);
}

void VideoEncoder::shutdown() {
    if (m_encoder) {
        m_encoder->ProcessMessage(MFT_MESSAGE_NOTIFY_END_OF_STREAM, 0);
        m_encoder->ProcessMessage(MFT_MESSAGE_COMMAND_DRAIN, 0);
    }

    m_encoder.Reset();
    m_inputType.Reset();
    m_outputType.Reset();

    if (m_initialized) {
        MFShutdown();
        m_initialized = false;
    }
}

} // namespace client
} // namespace cm
