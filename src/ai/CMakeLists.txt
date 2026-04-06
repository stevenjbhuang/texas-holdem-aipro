add_library(poker_ai STATIC
    OpenAICompatibleClient.cpp
    PromptBuilder.cpp
)

target_include_directories(poker_ai PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(poker_ai PRIVATE
    poker_core
    nlohmann_json::nlohmann_json
)

# httplib is PUBLIC because OpenAICompatibleClient.hpp includes httplib.h (needed for
# the persistent Client member). Consumers that include OpenAICompatibleClient.hpp
# need the httplib include path.
target_link_libraries(poker_ai PUBLIC
    httplib::httplib
)