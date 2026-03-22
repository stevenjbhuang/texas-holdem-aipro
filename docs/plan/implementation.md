# Texas Hold'em AI Pro — Implementation Plan

> **This is a guided learning plan.** Each phase introduces a concept, shows you a scaffold, and asks you to implement it. Only ask for the answer if you're stuck. The goal is that *you* write the code.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a playable Texas Hold'em game in C++ with SFML graphics and LLM-powered AI players via Ollama.

**Architecture:** Three decoupled layers (core / players / ai / ui) communicating through well-defined interfaces. `GameEngine` runs on a worker thread; SFML renders on the main thread.

**Tech Stack:** C++17, CMake 3.21+, SFML 2.6, cpp-httplib, nlohmann/json, yaml-cpp, Catch2 v3, PokerHandEvaluator, Ollama (local LLM)

---

## Phase 1 — CMake Scaffold & Project Structure

**Concepts you'll learn:** CMake basics, FetchContent, target-based dependency management, out-of-source builds.

### Task 1.1: Understand CMake before touching it

Read the guide before writing any CMake. This saves hours of confusion.

- [ ] Read: [CMake concepts you need](../guides/cmake-intro.md) ← we'll create this together as you go
- [ ] Understand the difference between `add_library`, `add_executable`, and `target_link_libraries`
- [ ] Understand what "out-of-source build" means and why we use `build/` as the build directory

**Key mental model:** CMake doesn't build your code — it generates build files (Makefiles, Ninja files) that actually build your code. You configure with CMake, then build with the generated system.

---

### Task 1.2: Create the root `CMakeLists.txt`

**Your turn.** Create `/home/x/dev/texas-holdem-aipro/CMakeLists.txt` with:

```cmake
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
```

- [ ] Create the file above
- [ ] Verify it looks right — don't try to build yet, we haven't created `src/` or `tests/`

---

### Task 1.3: Create the folder skeleton

- [ ] Create all directories from the design spec:

```bash
mkdir -p src/{core,players,ai,ui}
mkdir -p tests/{core,players,ai}
mkdir -p cmake
mkdir -p config/personalities
mkdir -p assets/{cards,fonts}
```

- [ ] Create placeholder files. `main.cpp` needs a minimal `main()` or the linker will fail:

```bash
echo 'int main() { return 0; }' > src/main.cpp
touch tests/core/.gitkeep
touch tests/players/.gitkeep
touch tests/ai/.gitkeep
```

- [ ] Commit:
```bash
git add -A && git commit -m "chore: add project skeleton and root CMakeLists.txt"
```

---

### Task 1.4: Add dependencies via FetchContent

**Concept:** `FetchContent` lets CMake download and build dependencies at configure time. No manual `git clone` or system package needed (except SFML's system deps).

Create `cmake/Dependencies.cmake`:

```cmake
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
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG        v0.18.1
)
FetchContent_MakeAvailable(httplib)

# ── Catch2 v3 (testing) ───────────────────────────────────
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.7.1
)
FetchContent_MakeAvailable(Catch2)

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
    GIT_TAG        master
)
FetchContent_Populate(PokerHandEvaluator)   # downloads but does NOT call add_subdirectory
# We wire it manually in Task 1.5
```

- [ ] Create the file above
- [ ] Do NOT try to build yet — we need `src/CMakeLists.txt` first

---

### Task 1.5: Custom CMake wrapper for PokerHandEvaluator

**Concept:** Some libraries don't have CMake support. You'll wrap them manually — this is a real-world CMake skill.

PokerHandEvaluator is a C library. We need to tell CMake how to compile it.

- [ ] After `FetchContent_Populate(PokerHandEvaluator)`, add this to `cmake/Dependencies.cmake`:

```cmake
# PokerHandEvaluator: manual library target
# FetchContent_Populate lowercases the name → variable is pokerevaluator_SOURCE_DIR
# We name the target poker_hand_evaluator (different from the FetchContent name)
# to avoid conflicts with CMake's internal registration.
add_library(poker_hand_evaluator STATIC
    ${pokerevaluator_SOURCE_DIR}/c/src/evaluator5.c
    ${pokerevaluator_SOURCE_DIR}/c/src/evaluator6.c
    ${pokerevaluator_SOURCE_DIR}/c/src/evaluator7.c
    ${pokerevaluator_SOURCE_DIR}/c/src/hash.c
    ${pokerevaluator_SOURCE_DIR}/c/src/hashtable.c
    ${pokerevaluator_SOURCE_DIR}/c/src/hashtable5.c
    ${pokerevaluator_SOURCE_DIR}/c/src/hashtable6.c
    ${pokerevaluator_SOURCE_DIR}/c/src/hashtable7.c
)
target_include_directories(poker_hand_evaluator PUBLIC
    ${pokerevaluator_SOURCE_DIR}/include
)
```

**Your turn:** Look up what `add_library(... STATIC ...)` means vs `SHARED`. Why do we use `STATIC` here?

- [ ] Add the block above to `cmake/Dependencies.cmake`

---

### Task 1.6: Create `src/CMakeLists.txt`

**Concept:** Each subdirectory can have its own `CMakeLists.txt`. The root one orchestrates; subdirectory ones define targets.

- [ ] Create `src/CMakeLists.txt`:

```cmake
# We'll add targets here as we build each layer.
# For now, just the main executable with a placeholder.

add_executable(texas-holdem src/main.cpp)

target_link_libraries(texas-holdem PRIVATE
    # layers will be linked here as we build them
)
```

**Wait** — this path is wrong. Think about it: `src/CMakeLists.txt` is already *inside* `src/`. So the path to `main.cpp` relative to this file is just `main.cpp`, not `src/main.cpp`.

**Your turn:** Fix the path and create the file correctly.

---

### Task 1.7: Create `tests/CMakeLists.txt`

- [ ] Create `tests/CMakeLists.txt`:

```cmake
# Catch2 provides a helper to register tests with CTest.
# We must add Catch2's extras directory to CMAKE_MODULE_PATH so
# include(Catch) can find the catch_discover_tests() function.
include(CTest)
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)

# Sub-test directories — uncomment as we add tests
# add_subdirectory(core)
# add_subdirectory(players)
# add_subdirectory(ai)
```

---

### Task 1.8: First configure & build

Now let's verify the scaffold compiles:

- [ ] Configure:
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
```

- [ ] Build:
```bash
cmake --build build -j$(nproc)
```

Expected: downloads dependencies, compiles successfully, produces `build/texas-holdem` binary.

- [ ] Run it:
```bash
./build/texas-holdem
```

Expected: exits immediately with code 0 (main returns 0), no crash.

- [ ] Commit:
```bash
git add -A && git commit -m "chore: wire up CMake with all dependencies via FetchContent"
```

---

## Phase 2 — Core Layer: Types, Card, Deck, Hand

**Concepts you'll learn:** Value types in C++, enums vs enum class, operator overloading, `std::mt19937` for randomness, header-only vs split header/source.

**Design reminder:** `core/` has **zero** dependencies on `ai/`, `players/`, or `ui/`. It's pure poker logic.

---

### Task 2.1: Define shared types in `Types.hpp`

**Concept:** Strong types prevent bugs. `enum class` is scoped and doesn't implicitly convert to `int` the way plain `enum` does.

- [ ] Create `src/core/Types.hpp`:

```cpp
#pragma once
#include <stdexcept>

namespace poker {

using PlayerId = int;  // 0-based seat index

enum class Suit { Hearts, Diamonds, Clubs, Spades };
enum class Rank { Two=2, Three, Four, Five, Six, Seven, Eight,
                  Nine, Ten, Jack, Queen, King, Ace };

enum class Street { PreFlop, Flop, Turn, River, Showdown };

struct Action {
    enum class Type { Fold, Call, Raise } type;
    int amount = 0;
    // Call does NOT carry an amount — GameEngine computes it from GameState.
    // amount is only meaningful for Raise. Engine clamps to [minRaise, playerStack].
};

} // namespace poker
```

**Note on placement:** `Suit` and `Rank` are defined here in `Types.hpp` rather than in `Card.hpp` (as mentioned in the spec) — this is intentional. Keeping them in `Types.hpp` means `Card.hpp` only needs one include, and other files that need `Rank` or `Suit` don't have to pull in all of `Card.hpp`.

**Your turn:** Why use `enum class` instead of plain `enum`? Try to articulate it before moving on. Check: what happens if you write `Suit::Hearts == 0` with each?

---

### Task 2.2: Implement `Card`

**Concept:** Value type — small, copyable, no heap allocation. Const methods for inspection. Free functions for string conversion.

- [ ] Create `src/core/Card.hpp` — write it yourself using this spec:
  - Holds a `Suit` and a `Rank`
  - `==` and `!=` operators
  - `<` operator (for sorting: order by Rank, break ties by Suit)
  - `std::string toString() const` (e.g. `"Ace of Spades"`)
  - Use `namespace poker`

- [ ] Create `src/core/Card.cpp` with the implementations

**Scaffold to get you started:**

```cpp
// Card.hpp
#pragma once
#include "Types.hpp"
#include <string>

namespace poker {

class Card {
public:
    Card(Rank rank, Suit suit);

    Rank getRank() const;
    Suit getSuit() const;
    std::string toString() const;

    bool operator==(const Card& other) const;
    bool operator!=(const Card& other) const;
    bool operator<(const Card& other) const;

private:
    Rank m_rank;
    Suit m_suit;
};

} // namespace poker
```

**Your turn:** Implement `Card.cpp`. Tips:
- Use a `switch` statement in `toString()` for rank and suit names
- For `operator<`: compare rank first, then suit as tiebreaker

---

### Task 2.3: Add `src/core/CMakeLists.txt` and wire up the core library

**Concept:** We build `core` as a static library target that other layers link against.

- [ ] Create `src/core/CMakeLists.txt`:

```cmake
add_library(core STATIC
    Card.cpp
    # more files added here as we create them
)

target_include_directories(core PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(core PUBLIC
    poker_hand_evaluator
)
```

**Your turn:** What does `PUBLIC` vs `PRIVATE` mean in `target_link_libraries`? When would you use each? (Hint: think about who needs to know about the dependency — just this target, or anyone who links against this target too?)

- [ ] Update `src/CMakeLists.txt` to include the core subdirectory:

```cmake
add_subdirectory(core)
```

- [ ] Build to verify no errors:
```bash
cmake --build build -j$(nproc)
```

---

### Task 2.4: Implement `Deck`

**Concept:** `std::mt19937` is the standard C++ Mersenne Twister PRNG. `std::shuffle` takes a range and a PRNG engine.

- [ ] Create `src/core/Deck.hpp` and `Deck.cpp`. Spec:
  - Constructor builds all 52 cards (nested loop over Suit × Rank)
  - `void shuffle()` — uses `std::mt19937` seeded from `std::random_device`
  - `Card deal()` — returns and removes the top card; throws if empty
  - `void reset()` — restores all 52 cards (call before shuffle for a new hand)
  - `int remaining() const`

**Your turn:** Implement both files. Tips:
- Use `std::vector<Card>` as the internal container
- `std::shuffle(vec.begin(), vec.end(), rng)` shuffles in-place
- `deal()` can return `back()` and then `pop_back()`

---

### Task 2.5: Implement `Hand`

Simple value type for a player's two hole cards.

- [ ] Create `src/core/Hand.hpp` (header-only is fine, it's trivial):

```cpp
#pragma once
#include "Card.hpp"

namespace poker {

struct Hand {
    Card first;
    Card second;

    Hand(Card first, Card second) : first(first), second(second) {}
};

} // namespace poker
```

- [ ] Add `Deck.cpp` to `src/core/CMakeLists.txt`'s source list

- [ ] Build and commit:
```bash
cmake --build build -j$(nproc)
git add -A && git commit -m "feat(core): add Types, Card, Deck, Hand"
```

---

## Phase 3 — Core Layer: GameState & GameEngine

**Concepts you'll learn:** State machines, STL containers (`std::map`, `std::vector`, `std::set`), const-correctness.

---

### Task 3.1: Implement `GameState`

- [ ] Create `src/core/GameState.hpp`. Use the spec from `docs/spec/design.md` as your guide — all fields are defined there. Implement it yourself.

**Scaffold:**
```cpp
#pragma once
#include "Types.hpp"
#include "Card.hpp"
#include "Hand.hpp"
#include <map>
#include <vector>
#include <set>

namespace poker {

struct GameState {
    // Your turn: add all fields from the design spec
};

} // namespace poker
```

**Your turn:** Fill in every field. Reference `docs/spec/design.md` → "core/GameState.hpp" section. Pay attention to types: `std::map<PlayerId, Hand>`, `std::set<PlayerId>`, etc.

---

### Task 3.2: Understand state machines before implementing `GameEngine`

**Concept:** A state machine moves through discrete states based on events/inputs. Our game states are: `PreFlop → Flop → Turn → River → Showdown → (new hand)`.

Before coding, answer these questions (look them up or think through them):
- What triggers the transition from PreFlop to Flop?
- What triggers the transition from River to Showdown?
- What makes a betting round "complete"?

Write your answers in a comment at the top of `GameEngine.hpp` — this forces you to understand the logic before writing it.

---

### Task 3.3: Define the `GameEngine` interface

- [ ] Create `src/core/GameEngine.hpp`:

```cpp
#pragma once
#include "GameState.hpp"
#include "../players/IPlayer.hpp"
#include <vector>
#include <memory>

namespace poker {

struct GameConfig {
    int numPlayers;
    int startingStack;
    int smallBlind;
    int bigBlind;
};

class GameEngine {
public:
    GameEngine(std::vector<std::unique_ptr<IPlayer>> players, GameConfig config);

    // Advance the game by one action (request from active player, apply it)
    void tick();

    // Get a snapshot of current game state (for renderer)
    const GameState& getState() const;

    bool isGameOver() const;

private:
    void startNewHand();
    void runBettingRound();
    void advanceStreet();
    void determineWinner();

    void rotateDealerButton();
    void postBlinds();
    std::vector<PlayerId> buildActionOrder() const;

    GameState m_state;
    GameConfig m_config;
    std::vector<std::unique_ptr<IPlayer>> m_players;
    Deck m_deck;
};

} // namespace poker
```

**Your turn:** Notice `IPlayer` is used here before it's fully implemented. For now, create a stub `src/players/IPlayer.hpp`. A bare forward declaration (`class IPlayer;`) is NOT enough — `std::unique_ptr<IPlayer>` needs to call the destructor, so the compiler requires the full class body. Your stub needs at minimum:

```cpp
#pragma once
namespace poker {
class IPlayer {
public:
    virtual ~IPlayer() = default;
};
} // namespace poker
```

We'll replace this with the full interface in Phase 6.

Also note: `GameEngine` needs both a `getState()` (const ref, internal) and a `getStateSnapshot()` (by-value copy, thread-safe for the renderer). Add both to the header now:

```cpp
const GameState& getState() const;      // internal use
GameState getStateSnapshot();           // mutex-protected copy for renderer

private:
    mutable std::mutex m_stateMutex;    // protects m_state for snapshot reads
```

---

### Task 3.4: Implement `GameEngine.cpp`

This is the most complex implementation task. Take it method by method.

- [ ] Implement `startNewHand()`:
  - Reset and shuffle the deck
  - Deal 2 cards to each non-folded player via `player->dealHoleCards(hand)`
  - Rotate dealer button
  - Post blinds
  - Set street to `PreFlop`
  - Build action order (UTG = 3rd seat from dealer pre-flop)

- [ ] Implement `runBettingRound()`:
  - Follow the algorithm in `docs/spec/design.md` → "Betting round algorithm"
  - Loop until all active players have acted AND all bets are equal

- [ ] Implement `advanceStreet()`:
  - Deal community cards (3 for Flop, 1 for Turn, 1 for River)
  - Reset `currentBets`
  - Rebuild action order (first active player left of dealer)
  - Run next betting round

- [ ] Implement `determineWinner()`:
  - Use `HandEvaluator` to score each remaining player's best hand
  - Award pot to winner
  - Reset for next hand

- [ ] Add `GameEngine.cpp` and `GameState.cpp` to `src/core/CMakeLists.txt`

- [ ] Build:
```bash
cmake --build build -j$(nproc)
```

- [ ] Commit:
```bash
git add -A && git commit -m "feat(core): add GameState and GameEngine state machine"
```

---

### Task 3.5: Implement `HandEvaluator`

**Concept:** Wrapping a C library in a C++ class. The library uses a lower score = stronger hand convention.

- [ ] Create `src/core/HandEvaluator.hpp`:

```cpp
#pragma once
#include "Card.hpp"
#include <vector>

namespace poker {

class HandEvaluator {
public:
    // Evaluate best 5-card hand from 7 cards (2 hole + 5 community)
    // Returns a score: lower value = stronger hand
    static int evaluate(const std::vector<Card>& sevenCards);

    // Compare two hands. Returns true if handA beats handB.
    static bool beats(const std::vector<Card>& handA, const std::vector<Card>& handB);
};

} // namespace poker
```

- [ ] Implement `HandEvaluator.cpp` using the PokerHandEvaluator C API:

```cpp
#include "HandEvaluator.hpp"
#include <phevaluator/phevaluator.h>  // PokerHandEvaluator header

namespace poker {

// Helper: convert our Card to the library's card integer
static int toLibCard(const Card& c) {
    // The library uses: rank * 4 + suit (0-indexed)
    int rank = static_cast<int>(c.getRank()) - 2;  // 0=Two, 12=Ace
    int suit = static_cast<int>(c.getSuit());       // 0-3
    return rank * 4 + suit;
}

int HandEvaluator::evaluate(const std::vector<Card>& cards) {
    // cards must have exactly 7 elements
    return evaluate_7cards(
        toLibCard(cards[0]), toLibCard(cards[1]),
        toLibCard(cards[2]), toLibCard(cards[3]),
        toLibCard(cards[4]), toLibCard(cards[5]),
        toLibCard(cards[6])
    ).value;
}

bool HandEvaluator::beats(const std::vector<Card>& a, const std::vector<Card>& b) {
    return evaluate(a) < evaluate(b);  // lower = stronger
}

} // namespace poker
```

**Your turn:** Add `HandEvaluator.cpp` to the core CMakeLists.

---

## Phase 4 — Core Tests with Catch2

**Concepts you'll learn:** Unit testing philosophy, test-first thinking, Catch2 syntax, testing state machines.

---

### Task 4.1: Understand Catch2 syntax

Catch2 tests look like this:

```cpp
#include <catch2/catch_test_macros.hpp>

TEST_CASE("Deck deals 52 unique cards", "[deck]") {
    poker::Deck deck;
    deck.shuffle();

    std::set<std::string> seen;
    for (int i = 0; i < 52; i++) {
        auto card = deck.deal();
        REQUIRE(seen.find(card.toString()) == seen.end());  // no duplicates
        seen.insert(card.toString());
    }
    REQUIRE_THROWS(deck.deal());  // 53rd deal throws
}
```

- `TEST_CASE("name", "[tag]")` — defines a test
- `REQUIRE(expr)` — fails and stops test if false
- `CHECK(expr)` — fails but continues test if false
- `REQUIRE_THROWS(expr)` — passes if expression throws

---

### Task 4.2: Write Card tests

- [ ] Create `tests/core/CMakeLists.txt`:

```cmake
add_executable(core_tests
    test_card.cpp
    test_deck.cpp
    test_hand_evaluator.cpp
    test_game_engine.cpp
)

target_link_libraries(core_tests PRIVATE
    core
    Catch2::Catch2WithMain
)

# CTest and Catch are already set up in the parent tests/CMakeLists.txt
catch_discover_tests(core_tests)
```

- [ ] Uncomment `add_subdirectory(core)` in `tests/CMakeLists.txt`

- [ ] Create `tests/core/test_card.cpp`. **Your turn** — write tests for:
  - Two cards with same rank and suit are equal
  - Cards with different ranks compare correctly with `<`
  - `toString()` returns expected string

---

### Task 4.3: Write Deck tests

- [ ] Create `tests/core/test_deck.cpp`. **Your turn** — write tests for:
  - Fresh deck has 52 cards
  - After dealing all 52, `deal()` throws
  - No duplicate cards after dealing all 52
  - `reset()` restores count to 52

---

### Task 4.4: Write HandEvaluator tests

- [ ] Create `tests/core/test_hand_evaluator.cpp`. **Your turn** — write tests for:
  - Royal flush beats straight flush
  - Four of a kind beats full house
  - Two identical hands tie (same score)

Tip: construct specific `Card` vectors manually to represent known hands.

---

### Task 4.5: Write GameEngine tests

`GameEngine` tests need concrete `IPlayer` implementations to pass in. Create a mock:

- [ ] Create `tests/core/MockPlayer.hpp`:

```cpp
#pragma once
#include "../../src/players/IPlayer.hpp"

class MockPlayer : public poker::IPlayer {
public:
    MockPlayer(poker::PlayerId id, const std::string& name, poker::Action::Type defaultAction)
        : m_id(id), m_name(name), m_action{defaultAction, 0} {}

    poker::PlayerId    getId()   const override { return m_id; }
    std::string        getName() const override { return m_name; }
    void               dealHoleCards(const poker::Hand&) override {}
    poker::Action      getAction(const poker::GameState&) override { return m_action; }

    void setAction(poker::Action a) { m_action = a; }

private:
    poker::PlayerId m_id;
    std::string     m_name;
    poker::Action   m_action;
};
```

- [ ] Create `tests/core/test_game_engine.cpp`. Test the state machine:
  - After `startNewHand()`, each player has 2 hole cards
  - After all community cards dealt, `street == Showdown`
  - Player who folds is in `foldedPlayers`

- [ ] Build and run all tests:

```bash
cmake --build build -j$(nproc)
cd build && ctest --output-on-failure
```

Expected: all tests pass.

- [ ] Commit:
```bash
git add -A && git commit -m "test(core): add Catch2 tests for Card, Deck, HandEvaluator, GameEngine"
```

---

## Phase 5 — AI Layer: ILLMClient, OllamaClient, PromptBuilder

**Concepts you'll learn:** Interface design (abstract base classes), dependency injection, HTTP clients, JSON parsing, prompt engineering.

---

### Task 5.1: Understand interface design in C++

**Concept:** A pure abstract class (interface) in C++ declares virtual methods with `= 0`. Any class that inherits from it MUST implement those methods. This lets you swap implementations without changing the calling code.

```cpp
class ILLMClient {
public:
    virtual ~ILLMClient() = default;                         // always virtual destructor
    virtual std::string sendPrompt(const std::string& prompt) = 0;  // pure virtual
};

class OllamaClient : public ILLMClient {
public:
    std::string sendPrompt(const std::string& prompt) override { /* ... */ }
};

class MockLLMClient : public ILLMClient {
public:
    std::string sendPrompt(const std::string& prompt) override {
        return "CALL";  // always calls — for testing
    }
};
```

**Your turn:** Why does `ILLMClient` need a virtual destructor? What happens without it?

---

### Task 5.2: Create `ILLMClient`

- [ ] Create `src/ai/ILLMClient.hpp` — you know the interface already from Task 5.1. Write it.

---

### Task 5.3: Create personality config files

Before implementing `PromptBuilder`, create the personality files it will read.

- [ ] Create `config/personalities/default.md`:

```markdown
# Default Player

You are a balanced Texas Hold'em poker player. You play solid, fundamental poker.

## Style
- Play tight-aggressive: only enter pots with strong hands, but bet confidently when you do.
- Fold weak hands preflop without hesitation.
- Consider pot odds before calling.

## Decision format
You MUST respond with exactly one of:
- FOLD
- CALL
- RAISE <amount>   (e.g. RAISE 100)

No explanation. No other text. Just the action.
```

- [ ] Create `config/personalities/aggressive.md` — write your own. Make this player raise often, bluff occasionally, rarely fold.

- [ ] Create `config/personalities/cautious.md` — conservative player, folds marginal hands, rarely bluffs.

---

### Task 5.4: Implement `PromptBuilder`

**Concept:** This class serializes `GameState` into a human-readable prompt. The quality of this class directly affects how well the AI plays.

- [ ] Create `src/ai/PromptBuilder.hpp`:

```cpp
#pragma once
#include "../core/GameState.hpp"
#include <string>

namespace poker {

class PromptBuilder {
public:
    // Builds a complete prompt for the given player.
    // personalityText: contents of the personality .md file
    static std::string build(const GameState& state,
                             PlayerId ownId,
                             const std::string& personalityText);

private:
    static std::string formatCards(const std::vector<Card>& cards);
    static std::string formatHand(const Hand& hand);
    static std::string formatBettingHistory(const GameState& state);
};

} // namespace poker
```

- [ ] Implement `src/ai/PromptBuilder.cpp`. **Your turn.** The prompt should include:
  1. Personality text (injected at top)
  2. Your hole cards
  3. Community cards (with street name)
  4. Pot size
  5. Each player's chip count and current bet
  6. Legal actions: `FOLD`, `CALL $X`, `RAISE $Y-$Z`
  7. Decision instruction from personality file

**Important:** Do NOT include other players' hole cards in the prompt. Only `state.holeCards[ownId]`.

---

### Task 5.5: Implement `OllamaClient`

**Concept:** `cpp-httplib` is a single-header HTTP library. Ollama exposes a REST API at `localhost:11434`.

Ollama's generate endpoint:
```
POST http://localhost:11434/api/generate
Body: { "model": "llama3.2", "prompt": "...", "stream": false }
Response: { "response": "CALL", ... }
```

- [ ] Create `src/ai/OllamaClient.hpp`:

```cpp
#pragma once
#include "ILLMClient.hpp"
#include <string>

namespace poker {

class OllamaClient : public ILLMClient {
public:
    OllamaClient(const std::string& model, const std::string& endpoint);

    std::string sendPrompt(const std::string& prompt) override;

private:
    std::string m_model;
    std::string m_endpoint;  // e.g. "http://localhost:11434"
};

} // namespace poker
```

- [ ] Implement `src/ai/OllamaClient.cpp`. **Your turn.** Hints:
  - `#include <httplib.h>` for cpp-httplib
  - `#include <nlohmann/json.hpp>` for JSON
  - Parse `m_endpoint` to split host and port for httplib's `Client`
  - On HTTP error, return empty string (caller handles fallback)

- [ ] Create `src/ai/CMakeLists.txt`:

```cmake
add_library(ai STATIC
    OllamaClient.cpp
    PromptBuilder.cpp
)

target_include_directories(ai PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(ai PUBLIC
    core
    nlohmann_json::nlohmann_json
    httplib::httplib
    # yaml-cpp belongs in main.cpp (config loading), not here
    # Note: cpp-httplib exports httplib::httplib as of v0.11+.
    # If you see a "target not found" error, use this fallback instead:
    #   target_include_directories(ai PUBLIC ${httplib_SOURCE_DIR})
)
```

**Note on `yaml-cpp`:** The FetchContent target for yaml-cpp is `yaml-cpp` (not `yaml-cpp::yaml-cpp`). It is not linked into the `ai` library because `OllamaClient` and `PromptBuilder` don't parse YAML — only `main.cpp` does. We'll link it there in Phase 8.

- [ ] Add `add_subdirectory(ai)` to `src/CMakeLists.txt`

- [ ] Build:
```bash
cmake --build build -j$(nproc)
```

- [ ] Commit:
```bash
git add -A && git commit -m "feat(ai): add ILLMClient, OllamaClient, PromptBuilder, personality configs"
```

---

## Phase 6 — Players Layer: IPlayer, HumanPlayer, AIPlayer

**Concepts you'll learn:** Pure virtual interfaces, `std::promise`/`std::future` for thread communication, dependency injection.

---

### Task 6.1: Understand `std::promise` and `std::future`

**Concept:** These are two halves of a one-shot communication channel across threads.

```cpp
std::promise<int> promise;
std::future<int> future = promise.get_future();

// On thread A (waits for value):
int value = future.get();  // blocks until promise is fulfilled

// On thread B (provides value):
promise.set_value(42);     // unblocks thread A
```

In our case: `HumanPlayer::getAction()` (on GameEngine thread) blocks on `future.get()`. `InputHandler` (on main/UI thread) calls `promise.set_value(action)` when the user clicks a button.

**Your turn:** What happens if `promise.set_value()` is never called? What does the blocking thread do?

---

### Task 6.2: Implement `IPlayer`

- [ ] Create `src/players/IPlayer.hpp` (replace the stub from Phase 3):

```cpp
#pragma once
#include "../core/Types.hpp"
#include "../core/Hand.hpp"
#include "../core/GameState.hpp"
#include <string>

namespace poker {

class IPlayer {
public:
    virtual ~IPlayer() = default;
    virtual PlayerId    getId()   const = 0;
    virtual std::string getName() const = 0;
    virtual void        dealHoleCards(const Hand& cards) = 0;
    virtual Action      getAction(const GameState& state) = 0;
};

} // namespace poker
```

---

### Task 6.3: Implement `HumanPlayer`

- [ ] Create `src/players/HumanPlayer.hpp`:

```cpp
#pragma once
#include "IPlayer.hpp"
#include <future>
#include <memory>

namespace poker {

class HumanPlayer : public IPlayer {
public:
    HumanPlayer(PlayerId id, const std::string& name);

    PlayerId    getId()   const override;
    std::string getName() const override;
    void        dealHoleCards(const Hand& cards) override;
    Action      getAction(const GameState& state) override;

    // Called by InputHandler on the UI thread to fulfill the action
    void provideAction(Action action);

private:
    PlayerId    m_id;
    std::string m_name;
    Hand        m_hand;

    std::promise<Action>        m_promise;
    std::shared_future<Action>  m_future;
    std::atomic<bool> m_waitingForInput{false};  // read on UI thread, written on engine thread
};

} // namespace poker
```

- [ ] Implement `src/players/HumanPlayer.cpp`. **Your turn.** Key points:
  - `getAction()`: replace `m_promise` with a fresh `std::promise<Action>` FIRST (before setting the flag), then store the new `future`, set `m_waitingForInput = true`, call `m_future.get()` (blocks), reset flag, return action
  - `provideAction()`: call `m_promise.set_value(action)` — this unblocks `getAction()`
  - **Thread safety:** Replace the promise BEFORE setting `m_waitingForInput`. If `InputHandler` checks the flag and calls `provideAction()` before the new promise is ready, `set_value()` lands on the old promise and `get()` hangs forever. The order matters.
  - `m_waitingForInput` is `std::atomic<bool>` because `InputHandler` reads it on the UI thread while `GameEngine` writes it on the engine thread. Plain `bool` here is a data race.

---

### Task 6.4: Implement `AIPlayer`

- [ ] Create `src/players/AIPlayer.hpp`:

```cpp
#pragma once
#include "IPlayer.hpp"
#include "../ai/ILLMClient.hpp"
#include <string>

namespace poker {

class AIPlayer : public IPlayer {
public:
    AIPlayer(PlayerId id, const std::string& name,
             ILLMClient& client, const std::string& personalityText);

    PlayerId    getId()   const override;
    std::string getName() const override;
    void        dealHoleCards(const Hand& cards) override;
    Action      getAction(const GameState& state) override;

private:
    Action parseResponse(const std::string& response) const;
    Action fallbackAction() const;

    PlayerId    m_id;
    std::string m_name;
    ILLMClient& m_client;  // reference, not owner
    std::string m_personalityText;
    Hand        m_hand;
};

} // namespace poker
```

- [ ] Implement `src/players/AIPlayer.cpp`. **Your turn.** Key points:
  - `getAction()`: call `PromptBuilder::build(state, m_id, m_personalityText)`, send to `m_client.sendPrompt()`, parse result
  - `parseResponse()`: look for "FOLD", "CALL", or "RAISE <amount>" in the response string
  - On empty response or parse failure: retry once, then return `fallbackAction()` which returns `Action::Call`

- [ ] Create `src/players/CMakeLists.txt`:

```cmake
add_library(players STATIC
    HumanPlayer.cpp
    AIPlayer.cpp
)
target_include_directories(players PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(players PUBLIC core ai)
```

- [ ] Add `add_subdirectory(players)` to `src/CMakeLists.txt`

---

### Task 6.5: AIPlayer tests with MockLLMClient

- [ ] Create `tests/players/MockLLMClient.hpp`:

```cpp
#pragma once
#include "../../src/ai/ILLMClient.hpp"

class MockLLMClient : public poker::ILLMClient {
public:
    std::string responseToReturn = "CALL";

    std::string sendPrompt(const std::string&) override {
        return responseToReturn;
    }
};
```

- [ ] Create `tests/players/CMakeLists.txt` and `tests/players/test_ai_player.cpp`

- [ ] **Your turn:** Write tests for:
  - When mock returns `"FOLD"`, `getAction()` returns `Action::Type::Fold`
  - When mock returns `"RAISE 200"`, action type is Raise and amount is 200
  - When mock returns garbage, AIPlayer falls back to Call

- [ ] Uncomment `add_subdirectory(players)` in `tests/CMakeLists.txt`

- [ ] Build and run tests:
```bash
cmake --build build -j$(nproc) && cd build && ctest --output-on-failure
```

- [ ] Commit:
```bash
git add -A && git commit -m "feat(players): add IPlayer, HumanPlayer, AIPlayer with tests"
```

---

## Phase 7 — UI Layer: SFML Setup, Renderer, Input

**Concepts you'll learn:** Game loop pattern, SFML basics, rendering from shared state, thread-safe state snapshots.

---

### Task 7.1: Understand the SFML game loop

**Concept:** A game loop runs continuously:
1. Poll events (keyboard, mouse, window close)
2. Update game state
3. Render

In our design, step 2 happens on the GameEngine thread. The main thread only does steps 1 and 3.

```cpp
while (window.isOpen()) {
    // 1. Poll events
    sf::Event event;
    while (window.pollEvent(event)) {
        if (event.type == sf::Event::Closed) window.close();
        inputHandler.handle(event);  // routes to HumanPlayer if it's their turn
    }

    // 3. Render (reads a snapshot of game state)
    GameState snapshot = engine.getStateSnapshot();  // mutex-protected copy
    window.clear();
    renderer.render(snapshot);
    window.display();
}
```

---

### Task 7.2: Implement `SetupScreen`

- [ ] Create `src/ui/SetupScreen.hpp` and `SetupScreen.cpp`

The setup screen shows before the game starts. It lets the user configure:
- Number of AI players (1–8)
- Starting stack
- Small blind / big blind

Returns a `GameConfig` struct when the user clicks "Start".

**Your turn:** Implement this using SFML's `sf::Text`, `sf::RectangleShape` for buttons, and `sf::Event` for input. Keep it simple — plain buttons and text input fields are enough.

---

### Task 7.3: Implement `GameRenderer`

- [ ] Create `src/ui/GameRenderer.hpp` and `GameRenderer.cpp`

**Your turn:** Implement `render(const GameState& state)`. Start minimal:
1. Draw the table background (green rectangle)
2. Draw each player's name and chip count as text
3. Draw community cards as labeled rectangles (e.g. "A♠")
4. Draw pot size
5. Draw the human player's hole cards
6. Draw "Thinking..." overlay when AI is active (use a flag passed in)

Don't worry about card sprites yet — use colored rectangles with text. You can add real card art later.

---

### Task 7.4: Implement `InputHandler`

- [ ] Create `src/ui/InputHandler.hpp` and `InputHandler.cpp`

The input handler:
- Holds a reference to `HumanPlayer`
- Draws Fold/Call/Raise buttons when it's the human's turn
- On button click, calls `humanPlayer.provideAction(action)`

**Your turn:** Implement button hit-testing (check if mouse click coordinates are inside button rectangle) and route to `HumanPlayer`.

- [ ] Create `src/ui/CMakeLists.txt`:

```cmake
add_library(ui STATIC
    SetupScreen.cpp
    GameRenderer.cpp
    InputHandler.cpp
)
target_include_directories(ui PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(ui PUBLIC core players sfml-graphics sfml-window sfml-system)
```

- [ ] Add `add_subdirectory(ui)` to `src/CMakeLists.txt`

- [ ] Build:
```bash
cmake --build build -j$(nproc)
```

- [ ] Commit:
```bash
git add -A && git commit -m "feat(ui): add SetupScreen, GameRenderer, InputHandler"
```

---

## Phase 8 — Wire It All Together in `main.cpp`

**Concepts you'll learn:** Dependency wiring, threading with `std::thread`, RAII for thread lifetime, config loading.

---

### Task 8.1: Load config from `game.yaml`

- [ ] Link `yaml-cpp` into the main executable in `src/CMakeLists.txt`:

```cmake
target_link_libraries(texas-holdem PRIVATE
    core ai players ui
    yaml-cpp          # FetchContent target name (not yaml-cpp::yaml-cpp)
)
```

- [ ] In `main.cpp`, read `config/game.yaml` using yaml-cpp:

```cpp
#include <yaml-cpp/yaml.h>

YAML::Node config = YAML::LoadFile("config/game.yaml");
std::string model    = config["llm"]["model"].as<std::string>();
std::string endpoint = config["llm"]["endpoint"].as<std::string>();
```

**Your turn:** Extract all needed config values: model, endpoint.

---

### Task 8.2: Construct all objects

- [ ] In `main.cpp`, construct in this order:
  1. `OllamaClient` with model + endpoint from config
  2. Load personality text from `config/personalities/default.md`
  3. Create `HumanPlayer` (id=0, name="You")
  4. Create N `AIPlayer` instances (configurable from `SetupScreen`)
  5. Build `vector<unique_ptr<IPlayer>>`
  6. Create `GameEngine` with players + config
  7. Create SFML `sf::RenderWindow`
  8. Create `GameRenderer`, `InputHandler`, `SetupScreen`

---

### Task 8.3: Run `GameEngine` on a worker thread

**Concept:** `std::thread` runs a function on a new OS thread. We must join it before `main()` exits to avoid undefined behavior.

```cpp
std::thread engineThread([&engine]() {
    while (!engine.isGameOver()) {
        engine.tick();
    }
});

// ... main SFML loop runs on this thread ...

engineThread.join();  // wait for engine to finish before exiting
```

**Your turn:** Wrap this correctly with exception safety. What happens if the engine thread throws? How do you handle that?

---

### Task 8.4: Implement the main game loop

- [ ] Wire up the full game loop:

```cpp
while (window.isOpen() && !engine.isGameOver()) {
    sf::Event event;
    while (window.pollEvent(event)) {
        if (event.type == sf::Event::Closed) window.close();
        inputHandler.handle(event);
    }

    GameState snapshot = engine.getStateSnapshot();  // thread-safe copy
    window.clear(sf::Color(34, 100, 34));             // poker green
    renderer.render(snapshot);
    window.display();
}
```

- [ ] Implement `GameEngine::getStateSnapshot()` — lock `m_stateMutex`, copy `m_state`, unlock, return copy. Also wrap every write to `m_state` inside `GameEngine::tick()` with the same mutex lock so reads and writes are always synchronized.

- [ ] Build and run:
```bash
cmake --build build -j$(nproc)
./build/texas-holdem
```

Expected: SetupScreen appears, after configuration the game starts with cards dealt and AI players taking turns.

- [ ] Commit:
```bash
git add -A && git commit -m "feat: wire up full game loop with threaded GameEngine"
```

---

## Phase 9 — Polish & AI Tuning

**This phase is open-ended — explore at your own pace.**

---

### Task 9.1: Tune the AI prompt

- [ ] Play a few hands. Does the AI make sensible decisions?
- [ ] Iterate on `config/personalities/default.md` and `PromptBuilder.cpp`
- [ ] Try different Ollama models: `llama3.2`, `mistral`, `phi3`

---

### Task 9.2: Add personality variety

- [ ] Assign different personality files to each AI player
- [ ] Add a new personality (e.g. `maniac.md` — raises every hand)
- [ ] Observe how different personalities affect the game

---

### Task 9.3: Improve the renderer

- [ ] Add real card sprites (download a free card asset set to `assets/cards/`)
- [ ] Show action history log on screen
- [ ] Animate chip movements

---

### Task 9.4: Add a raise input widget

- [ ] Add a slider or text input for the raise amount
- [ ] Clamp to `[minRaise, playerStack]`

---

## Reference: Build & Test Commands

```bash
# Configure (first time or after CMakeLists changes)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build build -j$(nproc)

# Run tests
cd build && ctest --output-on-failure

# Run the game
./build/texas-holdem

# Clean build
rm -rf build && cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
```
