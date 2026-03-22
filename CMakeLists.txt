cmake_minimum_required(VERSION 3.21)
project(TexasHoldemAIPro VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)   # use -std=c++17, not -std=gnu++17

# Put all FetchContent declarations in a separate file (cleaner)
include(cmake/Dependencies.cmake)

# Sub-targets (we'll add these as we build each layer)
add_subdirectory(src)
add_subdirectory(tests)