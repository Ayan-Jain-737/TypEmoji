cmake_minimum_required(VERSION 3.25)

# Test target
find_package(Qt6 REQUIRED COMPONENTS Core Test Widgets)

# Linux/X11 dependencies for tests
find_package(X11 REQUIRED)
find_library(XTEST_LIB Xtst REQUIRED)

# Enable AUTOMOC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

add_executable(EmojiTyperTests
    TestRunner.cpp
)

target_link_libraries(EmojiTyperTests PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Widgets
    ${X11_LIBRARIES}
    ${XTEST_LIB}
)

# Link against main library sources for testing
target_sources(EmojiTyperTests PRIVATE
    ../src/injector/EmojiInjector.cpp
    ../src/TargetState.cpp
    ../src/data/EmojiDatabase.cpp
    ../src/panel/EmojiModel.cpp
    ../src/settings/Settings.cpp
    ../src/hotkey/X11Hotkey.cpp
)

target_include_directories(EmojiTyperTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/injector
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/panel
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/tray
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/hotkey
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/data
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/settings
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ipc
)

# Add test
add_test(NAME EmojiTyperUnitTests COMMAND EmojiTyperTests)