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

---

## Evaluation

Run `/phase-verify` to compile and get automated feedback on your implementation.

### Build checklist

```bash
# 1. Configure (if not already done)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug

# 2. Build — should produce zero errors and zero warnings
cmake --build build -j$(nproc)

# 3. Run all tests
cd build && ctest --output-on-failure
```

Success looks like:
- `cmake --build` exits with code `0` and the `core_tests` binary appears under `build/tests/core/`
- `ctest` output ends with `100% tests passed, 0 tests failed`
- Each test suite is listed individually, e.g.:
  ```
  Test #1: DeckTest.Has52Cards ....... Passed
  Test #2: DeckTest.NoDuplicates ..... Passed
  Test #3: CardTest.EqualCards ....... Passed
  ```
- No `FAILED` lines anywhere in the output

If only some suites appear, check that `gtest_discover_tests(core_tests)` is present in `tests/core/CMakeLists.txt` and that `add_subdirectory(core)` is uncommented in `tests/CMakeLists.txt`.

### Concept checklist

Answer these honestly about your own code before moving on:

- [ ] Did I use `EXPECT_*` for most assertions and reserve `ASSERT_*` only for cases where a failure would cause a crash (e.g., dereferencing a null pointer)?
- [ ] Does my `test_deck.cpp` verify that dealing the 53rd card throws `std::out_of_range` using `EXPECT_THROW`?
- [ ] Does my `test_deck.cpp` use a `std::set` or similar structure to confirm no duplicate cards after dealing all 52?
- [ ] Does my `test_hand_evaluator.cpp` construct `Card` vectors manually (not rely on `Deck::deal()`) to create known hands?
- [ ] Did I cover at least three distinct hand rankings in `test_hand_evaluator.cpp` (e.g., royal flush > straight flush > four of a kind)?
- [ ] In `MockPlayer`, does `getAction()` return the stored `m_action` without touching any real game logic?
- [ ] In `test_game_engine.cpp`, does my fold test assert that the folded player appears in `foldedPlayers` (not just that the game continued)?
- [ ] Did I split tests into separate `.cpp` files per class, rather than putting all tests in one file?

### Common mistakes

| Mistake | Symptom | Fix |
|---|---|---|
| Forgetting `gtest_discover_tests(core_tests)` in CMakeLists | `cmake --build` succeeds but `ctest` reports "No tests were found" | Add `gtest_discover_tests(core_tests)` after `target_link_libraries` in `tests/core/CMakeLists.txt` |
| Using `ASSERT_EQ` where `EXPECT_EQ` is appropriate | A single mismatch in a loop aborts the whole test, hiding later failures | Use `EXPECT_EQ` inside loops; only use `ASSERT_*` before a pointer dereference or similar crash risk |
| `MockPlayer` stores `IPlayer*` instead of a concrete type | Linker error: undefined vtable or pure virtual call at runtime | `MockPlayer` must override all four pure virtual methods: `getId`, `getName`, `dealHoleCards`, `getAction` |
| Constructing hands via `Deck::deal()` in evaluator tests | Test is fragile — it depends on shuffle order and may produce unpredictable hands | Build `std::vector<Card>` directly using specific `Rank` and `Suit` values |
| `add_subdirectory(core)` left commented out in `tests/CMakeLists.txt` | `core_tests` target never defined; build error "No rule to make target" | Uncomment the line in `tests/CMakeLists.txt` |

### Self-score

- **Solid**: All tests green on first build attempt. You could explain what `EXPECT_` vs `ASSERT_` means and why `MockPlayer` needs a virtual destructor. All checklist boxes checked confidently.
- **Learning**: Tests pass after 1-2 CMake or linker fixes. You had to look up `gtest_discover_tests` or the `EXPECT_THROW` syntax. A few checklist items required re-reading the concept boxes.
- **Needs review**: Build failed with multiple errors (missing targets, linker failures, no tests discovered). Go back to Task 4.1, re-read the GoogleTest syntax section, and make sure your CMakeLists wiring matches the scaffold exactly before continuing to Phase 5.
