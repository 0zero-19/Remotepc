# ===========================================================
# client/ — StudentAgent executable
# Backend: DXGI capture + H.264 encoding + network
# ===========================================================

add_executable(StudentAgent WIN32
    src/main.cpp
    src/ScreenCapturer.cpp
    src/VideoEncoder.cpp
    src/InputInjector.cpp
    src/LockManager.cpp
)

target_include_directories(StudentAgent
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(StudentAgent
    PRIVATE
        CommonLib

        # DirectX / DXGI
        d3d11
        dxgi

        # Media Foundation (H.264)
        mfplat
        mfuuid
        mf
        mfreadwrite

        # Windows Imaging Component & COM
        windowscodecs
        ole32
        oleaut32

        # Win32
        user32
        gdi32
        shell32
)


# Определения для Winsock
target_compile_definitions(StudentAgent PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)
