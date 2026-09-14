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

include(GNUInstallDirs)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Concurrent
    Multimedia
    MultimediaWidgets
    LinguistTools
)

# Optional add-on: the file-manager preview service (space-bar preview in GNOME
# Files). It is built and installed by default, but it is never required - when
# Qt DBus is missing the viewer is built exactly as before.
option(FLASHVIEW_ENABLE_PREVIEWER "Build the file-manager preview service" ON)

set(FLASHVIEW_PREVIEWER ON)
if(FLASHVIEW_ENABLE_PREVIEWER)
    find_package(Qt6 QUIET COMPONENTS DBus)
    if(NOT Qt6DBus_FOUND)
        set(FLASHVIEW_PREVIEWER OFF)
        message(STATUS
            "Qt6 DBus not found: building without the file-manager preview service")
    endif()
else()
    set(FLASHVIEW_PREVIEWER OFF)
endif()

if(FLASHVIEW_PREVIEWER)
    set(PREVIEWER_SOURCES src/PreviewWindow.cpp src/PreviewerService.cpp)
    set(PREVIEWER_HEADERS src/PreviewWindow.h src/PreviewerService.h)
    message(STATUS "File-manager preview service: enabled")
else()
    set(PREVIEWER_SOURCES "")
    set(PREVIEWER_HEADERS "")
    message(STATUS "File-manager preview service: disabled")
endif()

# Applies the preview service to a target, or leaves it a plain viewer.
function(flashview_configure_previewer target)
    if(FLASHVIEW_PREVIEWER)
        target_link_libraries(${target} PRIVATE Qt6::DBus)
        target_compile_definitions(${target} PRIVATE FLASHVIEW_HAVE_PREVIEWER)
    endif()
endfunction()

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
    src/FlashViewStyle.h
)

qt_add_executable(flashview
    ${SOURCES}
    ${HEADERS}
    ${PREVIEWER_SOURCES}
    ${PREVIEWER_HEADERS}
    resources/resources.qrc
)
flashview_configure_previewer(flashview)

qt_add_translations(flashview TS_FILES ${TS_FILES}
    QM_FILES_OUTPUT_VARIABLE FLASHVIEW_QM_FILES)
qt_add_resources(flashview "translations"
    PREFIX "/i18n"
    BASE "${CMAKE_CURRENT_BINARY_DIR}"
    FILES ${FLASHVIEW_QM_FILES})

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

# D-Bus activation for the preview service. The file manager starts FlashView
# through this file the first time a preview is requested.
if(FLASHVIEW_PREVIEWER)
    set(FLASHVIEW_PREVIEWER_EXEC "${CMAKE_INSTALL_FULL_BINDIR}/flashview")
    configure_file(resources/org.gnome.NautilusPreviewer.service.in
        "${CMAKE_CURRENT_BINARY_DIR}/org.gnome.NautilusPreviewer.service" @ONLY)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/org.gnome.NautilusPreviewer.service"
        DESTINATION share/dbus-1/services)
endif()

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
        src/FlashViewStyle.h
        ${PREVIEWER_SOURCES}
        ${PREVIEWER_HEADERS}
    )
    flashview_configure_previewer(flashview_tests)
    target_include_directories(flashview_tests PRIVATE src)
    # Capture the same translated UI as the application, without relying on
    # translations being installed next to the test executable.
    qt_add_resources(flashview_tests "test_translations"
        PREFIX "/i18n"
        BASE "${CMAKE_CURRENT_BINARY_DIR}"
        FILES ${FLASHVIEW_QM_FILES})
    target_link_libraries(flashview_tests PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Concurrent
        Qt6::Test
        Qt6::Multimedia
        Qt6::MultimediaWidgets
    )

    # The preview service needs a session bus; a private one keeps the test off
    # the developer's own desktop session.
    find_program(DBUS_RUN_SESSION_EXECUTABLE dbus-run-session)
    if(FLASHVIEW_PREVIEWER AND DBUS_RUN_SESSION_EXECUTABLE)
        add_test(NAME flashview_tests
            COMMAND "${DBUS_RUN_SESSION_EXECUTABLE}" -- $<TARGET_FILE:flashview_tests>)
    else()
        add_test(NAME flashview_tests COMMAND flashview_tests)
    endif()
    set_tests_properties(flashview_tests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
