cmake_minimum_required(VERSION 3.21)
project(ClassroomMonitor VERSION 1.0.0 LANGUAGES CXX)

# ============================================
# Global settings
# ============================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Единая директория для всех скомпилированных .exe
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/Debug")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/Release")

# MSVC-specific flags
if(MSVC)
    add_compile_options(/W4 /utf-8)
    # Suppress specific warnings if needed
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    add_compile_definitions(UNICODE _UNICODE)
    add_compile_definitions(WIN32_LEAN_AND_MEAN)
    add_compile_definitions(NOMINMAX)
endif()

# ============================================
# Subprojects (CommonLib & StudentAgent)
# ============================================
add_subdirectory(common)
add_subdirectory(client)

# ============================================
# Qt6 — find package (optional for client-only builds)
# ============================================
find_package(Qt6 COMPONENTS Core Gui Widgets Network QUIET)
if(Qt6_FOUND)
    qt_standard_project_setup()
    add_subdirectory(server)
else()
    message(STATUS "Qt6 not found — TeacherPanel skipped (StudentAgent only)")
endif()

# ============================================
# Tests (optional)
# ============================================
option(BUILD_TESTS "Build unit tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

