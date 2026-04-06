include(FetchContent)

# ── nlohmann/json ─────────────────────────────────────────
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# ── yaml-cpp ──────────────────────────────────────────────
FetchContent_Declare(
    yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG        0.8.0
)
FetchContent_MakeAvailable(yaml-cpp)

# ── cpp-httplib ───────────────────────────────────────────
# HTTPLIB_USE_OPENSSL enables SSLClient for https:// endpoints.
# Requires: sudo apt install libssl-dev
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG        v0.18.1
)
set(HTTPLIB_USE_OPENSSL ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(httplib)

# ── GoogleTest (testing) ──────────────────────────────────
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)
FetchContent_MakeAvailable(googletest)

# ── SFML 2.6 ──────────────────────────────────────────────
# Note: SFML needs system libs. On Ubuntu/Debian:
#   sudo apt install libsfml-dev
# This FetchContent fetches SFML source but links against system OpenGL/freetype.
FetchContent_Declare(
    SFML
    GIT_REPOSITORY https://github.com/SFML/SFML.git
    GIT_TAG        2.6.2
)
FetchContent_MakeAvailable(SFML)

# ── PokerHandEvaluator ────────────────────────────────────
# This library needs a custom wrapper — see Task 1.5
FetchContent_Declare(
    PokerHandEvaluator
    GIT_REPOSITORY https://github.com/HenryRLee/PokerHandEvaluator.git
    GIT_TAG        v0.5.3.1
)
FetchContent_Populate(PokerHandEvaluator)   # downloads but does NOT call add_subdirectory
# We wire it manually in Task 1.5

# PokerHandEvaluator: manual library target
# FetchContent_Populate lowercases the name → variable is pokerhandevaluator_SOURCE_DIR
add_library(poker_hand_evaluator STATIC
    ${pokerhandevaluator_SOURCE_DIR}/cpp/src/evaluator7.c
    ${pokerhandevaluator_SOURCE_DIR}/cpp/src/evaluator6.c
    ${pokerhandevaluator_SOURCE_DIR}/cpp/src/evaluator5.c
    ${pokerhandevaluator_SOURCE_DIR}/cpp/src/dptables.c
    ${pokerhandevaluator_SOURCE_DIR}/cpp/src/tables_bitwise.c
    ${pokerhandevaluator_SOURCE_DIR}/cpp/src/hash.c
    ${pokerhandevaluator_SOURCE_DIR}/cpp/src/hashtable.c
    ${pokerhandevaluator_SOURCE_DIR}/cpp/src/hashtable5.c
    ${pokerhandevaluator_SOURCE_DIR}/cpp/src/hashtable6.c
    ${pokerhandevaluator_SOURCE_DIR}/cpp/src/hashtable7.c
    ${pokerhandevaluator_SOURCE_DIR}/cpp/src/7462.c
    ${pokerhandevaluator_SOURCE_DIR}/cpp/src/rank.c
)
target_include_directories(poker_hand_evaluator PUBLIC
    ${pokerhandevaluator_SOURCE_DIR}/cpp/include
)