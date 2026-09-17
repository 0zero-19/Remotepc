# ===========================================================
# server/ — TeacherPanel executable (Qt6)
# Frontend: GUI application for the teacher
# ===========================================================

# Qt6 auto-generation for moc/uic/rcc
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

add_executable(TeacherPanel WIN32
    src/main.cpp
    src/MainWindow.cpp

    src/StudentTile.cpp
    src/ClientSession.cpp
    src/StudentViewDialog.cpp
    src/ThemeManager.cpp
    src/ClientSidebar.cpp
    src/SettingsDialog.cpp
    src/MessageDialog.cpp
    include/server/MainWindow.h
    include/server/StudentTile.h
    include/server/ClientSession.h
    include/server/StudentViewDialog.h
    include/server/ThemeManager.h
    include/server/ClientSidebar.h
    include/server/SettingsDialog.h
    include/server/MessageDialog.h
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
