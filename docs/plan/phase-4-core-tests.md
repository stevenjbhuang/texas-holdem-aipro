# Phase 4 — Core Tests with GoogleTest

**Concepts you'll learn:** Unit testing philosophy, test-first thinking, GoogleTest syntax, testing state machines, mock objects.

**Previous phase:** [Phase 3 — GameState & GameEngine](phase-3-gamestate-gameengine.md)
**Next phase:** [Phase 5 — AI Layer](phase-5-ai-layer.md)

---

### Task 4.1: Understand GoogleTest syntax

GoogleTest tests look like this:

```cpp
#include <gtest/gtest.h>

TEST(DeckTest, Deals52UniqueCards) {
    poker::Deck deck;
    deck.shuffle();

    std::set<std::string> seen;
    for (int i = 0; i < 52; i++) {
        auto card = deck.deal();
        EXPECT_EQ(seen.find(card.toString()), seen.end());  // no duplicates
        seen.insert(card.toString());
    }
    EXPECT_THROW(deck.deal(), std::out_of_range);  // 53rd deal throws
}
```

- `TEST(SuiteName, TestName)` — defines a test; suite groups related tests
- `EXPECT_*(expr)` — fails but continues the test if the condition is not met
- `ASSERT_*(expr)` — fails and stops the test immediately
- `EXPECT_EQ(a, b)`, `EXPECT_TRUE(x)`, `EXPECT_THROW(expr, ExcType)` — common matchers

**`EXPECT_` vs `ASSERT_`:** prefer `EXPECT_` unless a failure makes subsequent lines meaningless (e.g., a null pointer dereference would follow).

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
    poker_core
    GTest::gtest_main
)

# Registers each TEST() with CTest automatically
gtest_discover_tests(core_tests)
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
git add -A && git commit -m "test(core): add GoogleTest tests for Card, Deck, HandEvaluator, GameEngine"
```
