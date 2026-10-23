cmake_minimum_required(VERSION 3.25)
project(EmojiTyper VERSION 1.0.0 LANGUAGES CXX)

# C++23
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Qt6 - using dynamic linking (LGPL)
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network Concurrent DBus)

# Enable AUTOMOC/AUTORCC/AUTOUIC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Linux/X11 dependencies
find_package(X11 REQUIRED)
find_library(XTEST_LIB Xtst REQUIRED)

# KDE Frameworks 6 - KGlobalAccel for Wayland native global shortcuts
find_package(KF6GlobalAccel REQUIRED)
find_package(KF6XmlGui REQUIRED)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Copy data files to build directory
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/bin/data/emoji-test.txt
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/bin/data
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/data/emoji-test.txt
        ${CMAKE_BINARY_DIR}/bin/data/emoji-test.txt
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/data/emoji-zwj-sequences.txt
        ${CMAKE_BINARY_DIR}/bin/data/emoji-zwj-sequences.txt
    COMMENT "Copying emoji data files to build directory"
)
add_custom_target(copy_data DEPENDS ${CMAKE_BINARY_DIR}/bin/data/emoji-test.txt)

# Main executable
add_executable(EmojiTyper
    src/main.cpp
    src/injector/EmojiInjector.cpp
    src/injector/EmojiInjector.h
    src/panel/EmojiPanel.cpp
    src/panel/EmojiPanel.h
    src/panel/EmojiModel.cpp
    src/panel/EmojiModel.h
    src/panel/EmojiSearchProxy.cpp
    src/panel/EmojiSearchProxy.h
    src/tray/TrayHost.cpp
    src/tray/TrayHost.h
    src/hotkey/GlobalHotkey.cpp
    src/hotkey/GlobalHotkey.h
    src/hotkey/KGlobalAccelHotkey.cpp
    src/hotkey/KGlobalAccelHotkey.h
    src/hotkey/X11Hotkey.cpp
    src/hotkey/X11Hotkey.h
    src/data/EmojiDatabase.cpp
    src/data/EmojiDatabase.h
    src/settings/Settings.cpp
    src/settings/Settings.h
    src/ipc/SingleInstance.cpp
    src/ipc/SingleInstance.h
    src/TargetState.cpp
    src/TargetState.h
    resources/resources.qrc
)

# Make EmojiTyper depend on copy_data
add_dependencies(EmojiTyper copy_data)

# Include directories
target_include_directories(EmojiTyper PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/injector
    ${CMAKE_CURRENT_SOURCE_DIR}/src/panel
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tray
    ${CMAKE_CURRENT_SOURCE_DIR}/src/hotkey
    ${CMAKE_CURRENT_SOURCE_DIR}/src/data
    ${CMAKE_CURRENT_SOURCE_DIR}/src/settings
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ipc
)

# Qt6 modules
target_link_libraries(EmojiTyper PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Concurrent
    Qt6::DBus
    KF6::GlobalAccel
    KF6::XmlGui
)

# X11 libraries
target_link_libraries(EmojiTyper PRIVATE
    ${X11_LIBRARIES}
    ${XTEST_LIB}
)

# Compiler warnings
if(MSVC)
    target_compile_options(EmojiTyper PRIVATE /W4 /WX)
else()
    target_compile_options(EmojiTyper PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

# Auto-update installed binary so running daemon hot-reloads without manual restart
if(EXISTS "$ENV{HOME}/.local/bin/EmojiTyper")
    add_custom_command(TARGET EmojiTyper POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:EmojiTyper> $ENV{HOME}/.local/bin/EmojiTyper
        COMMAND ${CMAKE_COMMAND} -E make_directory $ENV{HOME}/.local/share/icons/hicolor/scalable/apps
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/emojityper.svg $ENV{HOME}/.local/share/icons/hicolor/scalable/apps/emojityper.svg
        COMMENT "Auto-updating ~/.local/bin/EmojiTyper and system icon cache for instant daemon reload"
    )
endif()

# Install target
install(TARGETS EmojiTyper
    RUNTIME DESTINATION bin
)

# Install data files
install(FILES
    data/emoji-test.txt
    data/emoji-zwj-sequences.txt
    DESTINATION share/emojityper/data
)

# Install desktop entry
install(FILES
    org.emojiboard.EmojiTyper.desktop
    DESTINATION share/applications
)

# Install icon
install(FILES
    assets/icons/emojityper.svg
    DESTINATION share/icons/hicolor/scalable/apps
)

# Install systemd user service
install(FILES
    installer/emojityper.service
    DESTINATION lib/systemd/user
)

# Enable testing
enable_testing()
add_subdirectory(tests)

# CPack for packaging
set(CPACK_PACKAGE_NAME "EmojiTyper")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Direct emoji typing tool for Linux (X11/Wayland)")
set(CPACK_PACKAGE_VENDOR "EmojiTyper")
set(CPACK_GENERATOR "TGZ")
include(CPack)