add_library(poker_players STATIC
    HumanPlayer.cpp
    AIPlayer.cpp
)

target_include_directories(poker_players PUBLIC ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(poker_players PUBLIC
    poker_core
    poker_ai
)