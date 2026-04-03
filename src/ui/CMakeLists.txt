add_library(poker_ui STATIC
    SetupScreen.cpp
    GameRenderer.cpp
    InputHandler.cpp
    AnimationManager.cpp
    SoundManager.cpp
)

target_include_directories(poker_ui PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(poker_ui PUBLIC
    poker_core
    poker_players
    sfml-graphics
    sfml-window
    sfml-audio
    sfml-system
)
