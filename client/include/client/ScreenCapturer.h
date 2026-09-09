#pragma once
// =============================================================================
// ClassroomMonitor — Screen Capturer
//
// Захват экрана через DXGI Desktop Duplication API.
// Работает только на Windows 10/11 с поддержкой DirectX 11.
// =============================================================================

#include "common/VideoTypes.h"

#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include <memory>
#include <functional>

namespace cm {
namespace client {

using Microsoft::WRL::ComPtr;

class ScreenCapturer {
public:
    ScreenCapturer();
    ~ScreenCapturer();

    // Запрет копирования (COM ресурсы)
    ScreenCapturer(const ScreenCapturer&) = delete;
    ScreenCapturer& operator=(const ScreenCapturer&) = delete;

    /// Инициализация DXGI. Возвращает true при успехе.
    bool initialize(uint32_t outputIndex = 0);

    /// Захватить один кадр. Возвращает true если кадр получен.
    /// При отсутствии изменений на экране может вернуть false.
    bool captureFrame(video::RawFrame& outFrame);

    /// Получить разрешение экрана
    uint32_t getScreenWidth() const { return m_width; }
    uint32_t getScreenHeight() const { return m_height; }

    /// Освободить ресурсы
    void shutdown();

private:
    bool initD3D11();
    bool initDuplication(uint32_t outputIndex);

    ComPtr<ID3D11Device>            m_device;
    ComPtr<ID3D11DeviceContext>     m_context;
    ComPtr<IDXGIOutputDuplication>  m_duplication;
    ComPtr<ID3D11Texture2D>         m_stagingTexture;

    uint32_t m_width  = 0;
    uint32_t m_height = 0;
    bool     m_initialized = false;
};

} // namespace client
} // namespace cm
