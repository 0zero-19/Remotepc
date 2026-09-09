# ===========================================================
# common/ — Статическая библиотека CommonLib
# Общий протокол для client/ и server/
# ===========================================================

add_library(CommonLib STATIC
    src/Protocol.cpp
)

target_include_directories(CommonLib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(CommonLib PUBLIC cxx_std_17)

# Windows Sockets для сетевых типов
if(WIN32)
    target_link_libraries(CommonLib PUBLIC Ws2_32)
endif()
