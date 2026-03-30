add_executable(players_tests
    test_ai_player.cpp
)

target_include_directories(players_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/tests
)
target_link_libraries(players_tests PRIVATE
    poker_players
    GTest::gtest_main
)

gtest_discover_tests(players_tests)