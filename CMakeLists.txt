cmake_minimum_required(VERSION 3.21)
project(TexasHoldemAIPro VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)   # use -std=c++17, not -std=gnu++17

# Put all FetchContent declarations in a separate file (cleaner)
include(cmake/Dependencies.cmake)

# Output directories for binaries and libraries
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Enable CTest from the root so `ctest` finds all test subdirectories
enable_testing()

# Sub-targets (we'll add these as we build each layer)
add_subdirectory(src)
add_subdirectory(tests)

# Main executable target
add_executable(texas-holdem src/main.cpp)
target_link_libraries(texas-holdem PRIVATE
    poker_ui
    yaml-cpp
)