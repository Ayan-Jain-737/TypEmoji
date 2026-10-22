cmake_minimum_required(VERSION 3.16)
project(kglobalaccel_daemon LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core DBus)
find_package(KF6GlobalAccel REQUIRED)
find_package(KGlobalAccelD REQUIRED)

add_executable(kglobalacceld kglobalacceld.cpp)
target_link_libraries(kglobalacceld PRIVATE Qt6::Core Qt6::DBus KF6::GlobalAccel K::KGlobalAccelD)

install(TARGETS kglobalacceld DESTINATION bin)
