cmake_minimum_required(VERSION 3.16)
project(FlashView VERSION 1.0.0 LANGUAGES CXX)

# Default to an optimized Release build when the user does not specify one.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Concurrent
    Multimedia
    MultimediaWidgets
    LinguistTools
)

# Translation files
set(TS_FILES
    i18n/flashview_zh.ts
    i18n/flashview_en.ts
)

set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/ImageViewer.cpp
    src/VideoPlayer.cpp
    src/ThumbnailBar.cpp
    src/SettingsDialog.cpp
    src/ThemeManager.cpp
)

set(HEADERS
    src/MainWindow.h
    src/ImageViewer.h
    src/VideoPlayer.h
    src/ThumbnailBar.h
    src/SettingsDialog.h
    src/ThemeManager.h
    src/MediaUtils.h
)

qt_add_executable(flashview
    ${SOURCES}
    ${HEADERS}
    resources/resources.qrc
)

qt_add_translations(flashview TS_FILES ${TS_FILES})

target_link_libraries(flashview PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Concurrent
    Qt6::Multimedia
    Qt6::MultimediaWidgets
)

target_include_directories(flashview PRIVATE src)

install(TARGETS flashview DESTINATION bin)

# Desktop integration
install(FILES flashview.desktop
    DESTINATION share/applications)
install(FILES resources/icons/flashview.svg
    DESTINATION share/icons/hicolor/scalable/apps)

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)

    qt_add_executable(flashview_tests
        tests/FlashViewTests.cpp
        src/MainWindow.cpp
        src/MainWindow.h
        src/ImageViewer.cpp
        src/ImageViewer.h
        src/VideoPlayer.cpp
        src/VideoPlayer.h
        src/ThumbnailBar.cpp
        src/ThumbnailBar.h
        src/SettingsDialog.cpp
        src/SettingsDialog.h
        src/ThemeManager.cpp
        src/ThemeManager.h
        src/MediaUtils.h
    )
    target_include_directories(flashview_tests PRIVATE src)
    target_link_libraries(flashview_tests PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Concurrent
        Qt6::Test
        Qt6::Multimedia
        Qt6::MultimediaWidgets
    )

    add_test(NAME flashview_tests COMMAND flashview_tests)
    set_tests_properties(flashview_tests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
