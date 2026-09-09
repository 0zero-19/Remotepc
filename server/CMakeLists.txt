# ===========================================================
# server/ — TeacherPanel executable (Qt6)
# Frontend: GUI application for the teacher
# ===========================================================

# Qt6 auto-generation for moc/uic/rcc
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

add_executable(TeacherPanel
    src/main.cpp
    src/MainWindow.cpp
    src/StudentTile.cpp
    src/ClientSession.cpp
    src/ControlPanel.cpp
)

target_include_directories(TeacherPanel
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(TeacherPanel
    PRIVATE
        CommonLib
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Network
)

# Windows: не показывать консоль в Release
set_target_properties(TeacherPanel PROPERTIES
    WIN32_EXECUTABLE $<CONFIG:Release>
)
