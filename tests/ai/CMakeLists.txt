add_executable(ai_tests
    test_prompt_builder.cpp
)

target_link_libraries(ai_tests PRIVATE
    poker_ai
    GTest::gtest_main
)

gtest_discover_tests(ai_tests)