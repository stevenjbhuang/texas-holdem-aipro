# GoogleTest provides gtest_discover_tests() via the GoogleTest module.
# FetchContent_MakeAvailable(googletest) makes it available automatically.
include(CTest)
include(GoogleTest)

# Sub-test directories — uncomment as we add tests
add_subdirectory(core)
add_subdirectory(ai)
add_subdirectory(players)