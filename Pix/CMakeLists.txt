cmake_minimum_required(VERSION 3.10)
project(Pix VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find SFML (2.5+)
find_package(SFML 2.5 COMPONENTS graphics window system REQUIRED)

add_executable(Pix Pix.cpp)

target_link_libraries(Pix PRIVATE sfml-graphics sfml-window sfml-system)

# On Windows with MinGW, you may need to link additional libraries depending on your SFML build.
