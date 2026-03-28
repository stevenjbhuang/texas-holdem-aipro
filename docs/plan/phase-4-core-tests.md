# Phase 4 — Core Tests with GoogleTest

**Concepts you'll learn:** Unit testing philosophy, `EXPECT_` vs `ASSERT_`, test fixtures, testing exceptions, testing state machines, mock objects.

**Previous phase:** [Phase 3 — GameState & GameEngine](phase-3-gamestate-gameengine.md)
**Next phase:** [Phase 5 — AI Layer](phase-5-ai-layer.md)

**Design reminder:** Tests in `tests/core/` only depend on `poker_core`. No UI, no players, no AI.

---

### Task 4.1: Wire up the test build

Before writing any tests you need the CMake target and the subdirectory hook. Without these, `ctest` will find zero tests even if your `.cpp` files compile fine.

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

- [ ] Uncomment `add_subdirectory(core)` in `tests/CMakeLists.txt`.

- [ ] Verify the build wires up correctly (no tests written yet — just confirm it compiles):

```bash
cmake --build build -j$(nproc)
```

<details>
<summary>Concepts</summary>

> **Concept: `add_executable` for tests**
>
> Test binaries are executables, not libraries. `add_executable(core_tests ...)` creates a binary that, when run, executes all the tests and prints results. Each source file you list is compiled and linked into that binary:
>
> ```cmake
> add_executable(core_tests
>     test_card.cpp          # compiled into core_tests
>     test_deck.cpp          # compiled into core_tests
>     test_hand_evaluator.cpp
>     test_game_engine.cpp
> )
> ```
>
> All four files share one binary. GoogleTest discovers every `TEST()` macro across all of them at runtime.

> **Concept: `target_link_libraries` and `PRIVATE`**
>
> `target_link_libraries` does two things: it tells the linker which libraries to link, and it sets the visibility of that dependency.
>
> ```cmake
> target_link_libraries(core_tests PRIVATE
>     poker_core       # our game logic library
>     GTest::gtest_main # GoogleTest framework + main()
> )
> ```
>
> `PRIVATE` means these dependencies are only for `core_tests` itself — they are not propagated to any target that links against `core_tests`. Since nothing links against a test binary, `PRIVATE` is always correct here. The alternatives (`PUBLIC`, `INTERFACE`) are for library targets that re-export their dependencies to consumers.

> **Concept: `gtest_discover_tests` vs `add_test`**
>
> GoogleTest provides two ways to register tests with CTest:
>
> ```cmake
> # Old way: manually register each test by name
> add_test(NAME DeckTest.Has52Cards COMMAND core_tests --gtest_filter=DeckTest.Has52Cards)
>
> # Modern way: auto-discover everything
> gtest_discover_tests(core_tests)
> ```
>
> `gtest_discover_tests` runs `core_tests --gtest_list_tests` at CMake build time, reads the output, and registers every `TEST()` it finds with CTest automatically. You never have to update CMakeLists when you add a new test — just write the `TEST()` and it appears in `ctest` on the next build.
>
> `include(GoogleTest)` in `tests/CMakeLists.txt` is what makes `gtest_discover_tests` available — it loads the CMake module that defines that function.

> **Concept: `GTest::gtest_main` vs `GTest::gtest`**
>
> `GTest::gtest_main` links in a pre-written `main()` that initialises GoogleTest and runs all discovered tests. `GTest::gtest` links the framework only — you write `main()` yourself. Always use `gtest_main` unless you need custom startup logic (e.g. initialising a database connection before any test runs).

> **Concept: `include(CTest)` and `add_subdirectory`**
>
> `include(CTest)` in `tests/CMakeLists.txt` activates the CTest infrastructure for the build. Without it, `ctest` has no knowledge of any tests.
>
> `add_subdirectory(core)` tells CMake to descend into `tests/core/` and process its `CMakeLists.txt`. The sub-CMakeLists defines `core_tests` — without the `add_subdirectory` call, that target simply doesn't exist and the build produces no test binary.
>
> ```
> tests/CMakeLists.txt          ← include(CTest), add_subdirectory(core)
> tests/core/CMakeLists.txt     ← defines core_tests target
> tests/core/test_card.cpp      ← test source
> ```

**Questions — think through these before checking answers:**
1. `tests/CMakeLists.txt` has `include(GoogleTest)` at the top. If you remove that line, what breaks and when — at configure time or build time?
2. Why is the test binary called `core_tests` (one binary for all four files) rather than `card_tests`, `deck_tests`, etc. (one binary per file)?

</details>

<details>
<summary>Answers</summary>

**`tests/CMakeLists.txt` — uncommented:**

```cmake
# GoogleTest provides gtest_discover_tests() via the GoogleTest module.
# FetchContent_MakeAvailable(googletest) makes it available automatically.
include(CTest)
include(GoogleTest)

# Sub-test directories
add_subdirectory(core)
# add_subdirectory(players)
# add_subdirectory(ai)
```

**`tests/core/CMakeLists.txt`:**

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

---

**Q1.** At configure time. `gtest_discover_tests` is defined by the `GoogleTest` CMake module. If `include(GoogleTest)` is missing, CMake errors immediately when it hits the `gtest_discover_tests(core_tests)` line in `tests/core/CMakeLists.txt` — before any compilation starts.

**Q2.** One binary is simpler to maintain and faster to link. The cost of splitting into multiple binaries is separate link steps, separate ctest registrations, and separate CMake targets for no practical benefit — all four test files are testing the same `poker_core` library. The only reason to split would be if compilation time became a bottleneck (unlikely with four files) or if different binaries needed different dependencies.

</details>

---

### Task 4.2: Write Card tests

`Card` is a pure value type — it holds a rank and a suit, and provides string representations and comparison operators. These are the simplest tests in the project. The goal is to verify the public interface behaves exactly as documented: equality, ordering, and both string formats.

- [ ] Create `tests/core/test_card.cpp`. Write tests for:
  - Two cards with the same rank and suit are equal (`operator==`)
  - Two cards with different ranks are not equal (`operator!=`)
  - `operator<` orders by rank (Two < Ace)
  - `toString()` returns the full English string (e.g. `"Ace of Spades"`)
  - `toShortString()` returns the two-character format (e.g. `"As"`)

<details>
<summary>Concepts</summary>

> **Concept: `EXPECT_*` vs `ASSERT_*`**
>
> Both macros fail a test when their condition is not met. The difference is what happens next:
>
> ```cpp
> EXPECT_EQ(a, b);   // test continues even if this fails — you see all failures
> ASSERT_EQ(a, b);   // test stops immediately if this fails
> ```
>
> Use `ASSERT_*` only when continuing after a failure would crash or produce meaningless results — for example, asserting a pointer is not null before dereferencing it. Use `EXPECT_*` everywhere else so a single bad value doesn't hide all the other failures in the same test.

> **Concept: One concept per test**
>
> Each `TEST()` should verify exactly one behaviour. Compare:
>
> ```cpp
> // Bad: testing everything in one test — hard to diagnose on failure
> TEST(CardTest, Everything) {
>     Card c{Rank::Ace, Suit::Spades};
>     EXPECT_EQ(c.toString(), "Ace of Spades");
>     EXPECT_EQ(c.toShortString(), "As");
>     Card c2{Rank::Two, Suit::Clubs};
>     EXPECT_TRUE(c2 < c);
> }
>
> // Good: one behaviour, clear failure message
> TEST(CardTest, ToStringFullName)    { ... }
> TEST(CardTest, ToShortStringFormat) { ... }
> TEST(CardTest, LowerRankComesFirst) { ... }
> ```
>
> When a focused test fails, the name tells you exactly what is broken.

> **Concept: `EXPECT_EQ` vs `EXPECT_TRUE`**
>
> ```cpp
> EXPECT_TRUE(a == b);       // failure: "Value of: a == b  Actual: false  Expected: true"
> EXPECT_EQ(a, b);           // failure: "Expected: b  Actual: a"
> ```
>
> `EXPECT_EQ` prints both values on failure — much easier to diagnose. Use `EXPECT_EQ(a, b)` for equality, `EXPECT_LT(a, b)` for less-than, `EXPECT_TRUE(expr)` only when no dedicated matcher fits.

**Questions — think through these before checking answers:**
1. You have `EXPECT_EQ(card.toString(), "Ace of Spades")`. If `toString()` returns `"ace of spades"` (lowercase), what will GoogleTest print? How does that help you diagnose the bug faster than `EXPECT_TRUE`?
2. Why test `operator!=` separately from `operator==`? Can one be broken while the other works?

</details>

<details>
<summary>Answers</summary>

**Reference implementation:**

```cpp
// tests/core/test_card.cpp
#include <gtest/gtest.h>
#include "core/Card.hpp"

using namespace poker;

TEST(CardTest, EqualSameRankAndSuit) {
    Card a{Rank::Ace, Suit::Spades};
    Card b{Rank::Ace, Suit::Spades};
    EXPECT_EQ(a, b);
}

TEST(CardTest, NotEqualDifferentRank) {
    Card a{Rank::Ace,  Suit::Spades};
    Card b{Rank::King, Suit::Spades};
    EXPECT_NE(a, b);
}

TEST(CardTest, LowerRankComesFirst) {
    Card two{Rank::Two, Suit::Clubs};
    Card ace{Rank::Ace, Suit::Clubs};
    EXPECT_LT(two, ace);
}

TEST(CardTest, ToStringFullName) {
    Card c{Rank::Ace, Suit::Spades};
    EXPECT_EQ(c.toString(), "Ace of Spades");
}

TEST(CardTest, ToShortStringFormat) {
    EXPECT_EQ(Card(Rank::Ace,   Suit::Spades).toShortString(),   "As");
    EXPECT_EQ(Card(Rank::Ten,   Suit::Hearts).toShortString(),   "Th");
    EXPECT_EQ(Card(Rank::Two,   Suit::Clubs).toShortString(),    "2c");
    EXPECT_EQ(Card(Rank::King,  Suit::Diamonds).toShortString(), "Kd");
}
```

---

**Q1.** `EXPECT_EQ` prints: `Expected: "Ace of Spades"  Actual: "ace of spades"`. The two strings are right next to each other — you see the case difference immediately. `EXPECT_TRUE(card.toString() == "Ace of Spades")` only prints `"false"`, giving you nothing to work with.

**Q2.** Yes. `operator==` and `operator!=` are implemented independently. A developer could write `operator!=` as `return !(m_rank == other.m_rank)` — accidentally omitting the suit check — while `operator==` checks both fields correctly. Testing them separately catches this class of bug.

</details>

---

### Task 4.3: Write Deck tests

`Deck` is a stateful object — it changes every time you call `deal()`. Testing stateful objects requires thinking about *sequences* of operations, not just single calls. The four tests below cover the most important invariants: initial state, exhaustion, uniqueness, and reset.

- [ ] Create `tests/core/test_deck.cpp`. Write tests for:
  - A fresh deck has exactly 52 cards
  - After dealing all 52, `deal()` throws `std::out_of_range`
  - No duplicate cards after dealing all 52 (use `std::set`)
  - After `reset()`, the deck has 52 cards again and can be dealt from

<details>
<summary>Concepts</summary>

> **Concept: `EXPECT_THROW`**
>
> Tests that an expression throws a specific exception type:
>
> ```cpp
> EXPECT_THROW(deck.deal(), std::out_of_range);
> ```
>
> The test passes if `deck.deal()` throws exactly `std::out_of_range`. If it throws a different exception or doesn't throw at all, the test fails. This is the correct way to test error paths — not wrapping the call in a try/catch yourself.

> **Concept: Using `std::set` to detect duplicates**
>
> A set rejects duplicate insertions — its size stays at the number of *unique* elements:
>
> ```cpp
> std::set<std::string> seen;
> for (int i = 0; i < 52; i++)
>     seen.insert(deck.deal().toShortString());
> EXPECT_EQ(seen.size(), 52u);  // if any duplicate, size < 52
> ```
>
> This is more precise than checking for collisions manually. If two cards are identical, `seen.size()` will be 51 and the test failure message tells you exactly how many duplicates there were.

> **Concept: Test the state machine, not just individual calls**
>
> `reset()` is a transition in `Deck`'s state machine: exhausted → fresh. Testing it means verifying the state *after* the transition, not just that the call doesn't crash:
>
> ```cpp
> // Weak: only tests that reset() doesn't throw
> deck.reset();
>
> // Strong: tests the resulting state is correct
> deck.reset();
> EXPECT_EQ(deck.remainingCards(), 52);  // if Deck exposes this
> // or: deal all 52 and confirm no throw
> ```

**Questions — think through these before checking answers:**
1. Your duplicate test shuffles before dealing. Does it still work if you *don't* shuffle? Should the test shuffle or not?
2. After `reset()`, should you verify the deck is *different* from before (i.e., shuffle was re-applied), or just that 52 cards are available again?

</details>

<details>
<summary>Answers</summary>

**Reference implementation:**

```cpp
// tests/core/test_deck.cpp
#include <gtest/gtest.h>
#include "core/Deck.hpp"
#include <set>
#include <string>

using namespace poker;

TEST(DeckTest, FreshDeckHas52Cards) {
    Deck deck;
    int count = 0;
    while (true) {
        try { deck.deal(); ++count; }
        catch (const std::out_of_range&) { break; }
    }
    EXPECT_EQ(count, 52);
}

TEST(DeckTest, DealThrowsWhenExhausted) {
    Deck deck;
    for (int i = 0; i < 52; ++i) deck.deal();
    EXPECT_THROW(deck.deal(), std::out_of_range);
}

TEST(DeckTest, NoDuplicatesAfterShuffleAndDeal) {
    Deck deck;
    deck.shuffle();
    std::set<std::string> seen;
    for (int i = 0; i < 52; ++i)
        seen.insert(deck.deal().toShortString());
    EXPECT_EQ(seen.size(), 52u);
}

TEST(DeckTest, ResetRestores52Cards) {
    Deck deck;
    deck.shuffle();
    for (int i = 0; i < 10; ++i) deck.deal();
    deck.reset();
    // Should be able to deal 52 cards again without throwing
    EXPECT_NO_THROW({
        for (int i = 0; i < 52; ++i) deck.deal();
    });
    EXPECT_THROW(deck.deal(), std::out_of_range);
}
```

---

**Q1.** The duplicate test works with or without a shuffle — the deck always contains exactly 52 distinct cards regardless of order. Whether you shuffle is irrelevant to uniqueness. That said, shuffling exercises more code paths (shuffle + deal vs just deal), which makes it a slightly more realistic test.

**Q2.** You only need to verify that 52 cards are available. Verifying the *order changed* is a test of `shuffle()`, not `reset()`. Mixing concerns makes tests harder to diagnose. If you want to test that `shuffle()` randomises, write a separate test for that (though non-deterministic tests are tricky — a common approach is to shuffle twice with different seeds and confirm the orders differ).

</details>

---

### Task 4.4: Write HandEvaluator tests

`HandEvaluator` wraps a C library that assigns numeric scores to poker hands (lower = stronger). The key insight for testing it: **construct hands manually** using specific `Card` values. Never use `Deck::deal()` in evaluator tests — the result depends on shuffle order and is unpredictable.

- [ ] Create `tests/core/test_hand_evaluator.cpp`. Write tests for:
  - Royal flush beats straight flush
  - Four of a kind beats full house
  - Full house beats flush
  - Flush beats straight
  - Two identical 7-card hands produce the same score (tie)

<details>
<summary>Concepts</summary>

> **Concept: Constructing known hands**
>
> Build your test vectors card by card:
>
> ```cpp
> // Royal flush: A K Q J T of spades + two fillers
> std::vector<Card> royalFlush = {
>     Card{Rank::Ace,   Suit::Spades},
>     Card{Rank::King,  Suit::Spades},
>     Card{Rank::Queen, Suit::Spades},
>     Card{Rank::Jack,  Suit::Spades},
>     Card{Rank::Ten,   Suit::Spades},
>     Card{Rank::Two,   Suit::Hearts},   // filler
>     Card{Rank::Three, Suit::Diamonds}, // filler
> };
> ```
>
> The evaluator finds the best 5-card hand from all 7, so fillers don't affect the result as long as they don't accidentally improve it (e.g. don't add a sixth spade if you want to test a flush specifically).

> **Concept: Testing relative order, not absolute scores**
>
> The library's scores are opaque integers — you don't know that a royal flush scores exactly `1`. What you *do* know is the relative ordering of hand categories. Test that:
>
> ```cpp
> EXPECT_LT(HandEvaluator::evaluate(royalFlush), HandEvaluator::evaluate(straightFlush));
> // or use the helper:
> EXPECT_TRUE(HandEvaluator::beats(royalFlush, straightFlush));
> ```
>
> This makes your tests robust to library version changes that might renumber the scores.

> **Concept: Testing ties**
>
> Two identical hands must produce the same score. This verifies the encoding is deterministic and that no randomness crept in:
>
> ```cpp
> EXPECT_EQ(HandEvaluator::evaluate(handA), HandEvaluator::evaluate(handA));
> ```

**Questions — think through these before checking answers:**
1. You build a "flush" test hand with 5 spades and 2 filler cards. You accidentally add a 6th spade as a filler. Does this affect the test result?
2. `beats(a, b)` returns `evaluate(a) < evaluate(b)`. If two hands tie, what does `beats` return? Is that correct?

</details>

<details>
<summary>Answers</summary>

**Reference implementation:**

```cpp
// tests/core/test_hand_evaluator.cpp
#include <gtest/gtest.h>
#include "core/HandEvaluator.hpp"
#include "core/Card.hpp"

using namespace poker;

// Helpers to build known 7-card hands
static std::vector<Card> makeRoyalFlush() {
    return {
        Card{Rank::Ace,   Suit::Spades}, Card{Rank::King,  Suit::Spades},
        Card{Rank::Queen, Suit::Spades}, Card{Rank::Jack,  Suit::Spades},
        Card{Rank::Ten,   Suit::Spades},
        Card{Rank::Two,   Suit::Hearts}, Card{Rank::Three, Suit::Diamonds},
    };
}
static std::vector<Card> makeStraightFlush() {
    return {
        Card{Rank::Nine,  Suit::Hearts}, Card{Rank::Eight, Suit::Hearts},
        Card{Rank::Seven, Suit::Hearts}, Card{Rank::Six,   Suit::Hearts},
        Card{Rank::Five,  Suit::Hearts},
        Card{Rank::Two,   Suit::Clubs},  Card{Rank::Three, Suit::Diamonds},
    };
}
static std::vector<Card> makeFourOfAKind() {
    return {
        Card{Rank::Ace, Suit::Spades}, Card{Rank::Ace, Suit::Hearts},
        Card{Rank::Ace, Suit::Clubs},  Card{Rank::Ace, Suit::Diamonds},
        Card{Rank::King,Suit::Spades},
        Card{Rank::Two, Suit::Hearts}, Card{Rank::Three, Suit::Clubs},
    };
}
static std::vector<Card> makeFullHouse() {
    return {
        Card{Rank::King, Suit::Spades}, Card{Rank::King, Suit::Hearts},
        Card{Rank::King, Suit::Clubs},
        Card{Rank::Ace,  Suit::Spades}, Card{Rank::Ace,  Suit::Hearts},
        Card{Rank::Two,  Suit::Clubs},  Card{Rank::Three, Suit::Diamonds},
    };
}
static std::vector<Card> makeFlush() {
    return {
        Card{Rank::Ace,  Suit::Clubs}, Card{Rank::Jack, Suit::Clubs},
        Card{Rank::Nine, Suit::Clubs}, Card{Rank::Six,  Suit::Clubs},
        Card{Rank::Two,  Suit::Clubs},
        Card{Rank::King, Suit::Hearts}, Card{Rank::Three, Suit::Diamonds},
    };
}
static std::vector<Card> makeStraight() {
    return {
        Card{Rank::Nine, Suit::Spades}, Card{Rank::Eight, Suit::Hearts},
        Card{Rank::Seven,Suit::Clubs},  Card{Rank::Six,   Suit::Diamonds},
        Card{Rank::Five, Suit::Spades},
        Card{Rank::Two,  Suit::Hearts}, Card{Rank::Three, Suit::Clubs},
    };
}

TEST(HandEvaluatorTest, RoyalFlushBeatsStraightFlush) {
    EXPECT_TRUE(HandEvaluator::beats(makeRoyalFlush(), makeStraightFlush()));
}
TEST(HandEvaluatorTest, FourOfAKindBeatsFullHouse) {
    EXPECT_TRUE(HandEvaluator::beats(makeFourOfAKind(), makeFullHouse()));
}
TEST(HandEvaluatorTest, FullHouseBeatsFlush) {
    EXPECT_TRUE(HandEvaluator::beats(makeFullHouse(), makeFlush()));
}
TEST(HandEvaluatorTest, FlushBeatsStraight) {
    EXPECT_TRUE(HandEvaluator::beats(makeFlush(), makeStraight()));
}
TEST(HandEvaluatorTest, IdenticalHandsTie) {
    auto hand = makeRoyalFlush();
    EXPECT_EQ(HandEvaluator::evaluate(hand), HandEvaluator::evaluate(hand));
}
```

---

**Q1.** Yes, it can affect the result. With 6 spades the evaluator might find a higher-ranked flush (6 spades gives more combinations to choose from). Always use non-suit-matching fillers when testing a specific hand category.

**Q2.** `beats(a, b)` returns `evaluate(a) < evaluate(b)`. On a tie, both scores are equal so it returns `false` — meaning "a does not beat b." That is correct: a tie is not a win. `beats(b, a)` also returns `false`. If you need to detect a tie, check `!beats(a,b) && !beats(b,a)`.

</details>

---

### Task 4.5: Create `MockPlayer` and write GameEngine tests

`GameEngine` depends on `IPlayer` — an abstract interface. You can't instantiate it directly, and you don't want to use real `HumanPlayer` or `AIPlayer` in unit tests (they have threading and network dependencies). A `MockPlayer` is a minimal concrete `IPlayer` that returns a fixed action, giving you full control over what happens in the test.

- [ ] Create `tests/core/MockPlayer.hpp`:

```cpp
#pragma once
#include "players/IPlayer.hpp"

class MockPlayer : public poker::IPlayer {
public:
    MockPlayer(poker::PlayerId id, poker::Action::Type defaultAction)
        : m_id(id), m_action{defaultAction, 0} {}

    poker::PlayerId getId()                               const override { return m_id; }
    void            dealHoleCards(const poker::Hand&)           override {}
    poker::Action   getAction(const poker::PlayerView&)         override { return m_action; }

    void setAction(poker::Action a) { m_action = a; }

private:
    poker::PlayerId m_id;
    poker::Action   m_action;
};
```

- [ ] Create `tests/core/test_game_engine.cpp`. Write tests for:
  - After construction, each player has a non-zero chip count
  - Blinds are deducted from the correct players' stacks on construction
  - After construction, each active player has hole cards in the snapshot
  - A player whose `MockPlayer` always folds ends up in `foldedPlayers`
  - Pot is non-zero after blinds are posted

<details>
<summary>Concepts</summary>

> **Concept: Dependency injection for testability**
>
> `GameEngine` takes `vector<unique_ptr<IPlayer>>` — not concrete types. This is dependency injection: the engine doesn't know or care whether the players are human, AI, or mock. Tests inject `MockPlayer` instances; production code injects real players. The engine's logic is tested in isolation.
>
> ```cpp
> std::vector<std::unique_ptr<poker::IPlayer>> players;
> players.push_back(std::make_unique<MockPlayer>(0, Action::Type::Call));
> players.push_back(std::make_unique<MockPlayer>(1, Action::Type::Call));
> poker::GameEngine engine(std::move(players), config);
> ```

> **Concept: Testing via `getStateSnapshot()`**
>
> `GameEngine` is a black box — its internal `m_state` is private. `getStateSnapshot()` is the public observation point. The constructor initialises data only; `tick()` drives progression. Tests observe state at two natural points:
>
> ```cpp
> GameEngine engine(std::move(players), config);
> auto before = engine.getStateSnapshot();  // clean initial state — no hand in progress
> // assert initial chip counts, etc.
>
> engine.tick();                            // starts hand 1, runs pre-flop betting
> auto after = engine.getStateSnapshot();   // post-betting state
> // assert folded players, chip conservation, hole cards, etc.
> ```
>
> This is the correct pattern: test behaviour through the public interface, and only assert invariants that are guaranteed to hold at the observation point you choose.

> **Concept: `make_unique` and move semantics**
>
> `unique_ptr` cannot be copied — only moved. When building the players vector for the test, you must `std::move` it into the engine constructor:
>
> ```cpp
> // Won't compile — unique_ptr is not copyable
> engine = GameEngine(players, config);
>
> // Correct — transfers ownership
> engine = GameEngine(std::move(players), config);
> ```
>
> After the move, `players` is empty. That is intentional: the engine now owns the players.

**Questions — think through these before checking answers:**
1. `MockPlayer::dealHoleCards` does nothing — it ignores the cards. Does this break `GameEngine`? Who actually stores the hole cards?
2. If both `MockPlayer`s always call, does the betting round ever end? What terminates it?

</details>

<details>
<summary>Answers</summary>

**Reference implementation:**

```cpp
// tests/core/test_game_engine.cpp
#include <gtest/gtest.h>
#include "core/GameEngine.hpp"
#include "MockPlayer.hpp"

using namespace poker;

static GameConfig twoPlayerConfig() {
    GameConfig cfg;
    cfg.numPlayers    = 2;
    cfg.startingStack = 200;
    cfg.smallBlind    = 1;
    cfg.bigBlind      = 2;
    return cfg;
}

static std::vector<std::unique_ptr<IPlayer>> makePlayers(
    Action::Type p0, Action::Type p1)
{
    std::vector<std::unique_ptr<IPlayer>> players;
    players.push_back(std::make_unique<MockPlayer>(0, p0));
    players.push_back(std::make_unique<MockPlayer>(1, p1));
    return players;
}

TEST(GameEngineTest, InitialChipsBeforeFirstTick) {
    auto cfg = twoPlayerConfig();
    GameEngine engine(makePlayers(Action::Type::Call, Action::Type::Call), cfg);
    // Constructor must not run any game logic — all stacks at starting value
    auto state = engine.getStateSnapshot();
    for (auto& [id, chips] : state.chipCounts)
        EXPECT_EQ(chips, cfg.startingStack);
}

TEST(GameEngineTest, ChipConservationAfterHand) {
    auto cfg = twoPlayerConfig();
    GameEngine engine(makePlayers(Action::Type::Call, Action::Type::Call), cfg);
    engine.tick();
    auto state = engine.getStateSnapshot();
    int total = 0;
    for (auto& [id, chips] : state.chipCounts) total += chips;
    EXPECT_EQ(total + state.pot, cfg.startingStack * cfg.numPlayers);
}

TEST(GameEngineTest, EachActivePlayerHasHoleCards) {
    auto cfg = twoPlayerConfig();
    GameEngine engine(makePlayers(Action::Type::Call, Action::Type::Call), cfg);
    engine.tick();
    auto state = engine.getStateSnapshot();
    for (auto& [id, chips] : state.chipCounts)
        EXPECT_EQ(state.holeCards.count(id), 1u);
}

TEST(GameEngineTest, FoldingPlayerAddedToFoldedSet) {
    auto cfg = twoPlayerConfig();
    GameEngine engine(makePlayers(Action::Type::Fold, Action::Type::Call), cfg);
    engine.tick();
    auto state = engine.getStateSnapshot();
    EXPECT_TRUE(state.foldedPlayers.count(0) > 0 ||
                state.foldedPlayers.count(1) > 0);
}
```

---

**Q1.** No, it doesn't break `GameEngine`. `GameEngine` stores hole cards in `m_state.holeCards` — the map inside the engine itself. `player->dealHoleCards(hand)` is a notification to the player object (so a real `HumanPlayer` can display its cards, an `AIPlayer` can cache them). The engine's copy of the cards is unaffected. In tests, the notification can safely be ignored.

**Q2.** Yes, it terminates. `runBettingRound()` uses a `needsToAct` set. Each player calls once — removing themselves from `needsToAct`. Once the set is empty, the loop exits. If both players call the big blind without a raise, `needsToAct` becomes empty after two calls and the round ends.

</details>

---

### Task 4.6: Build and run all tests

- [ ] Build and run:

```bash
cmake --build build -j$(nproc) && cd build && ctest --output-on-failure
```

Expected output ends with:
```
100% tests passed, 0 tests failed out of N
```

- [ ] Commit:

```bash
git add tests/core/ tests/CMakeLists.txt
git commit -m "test(core): add GoogleTest tests for Card, Deck, HandEvaluator, GameEngine"
```

---

## Evaluation

Run `/phase-verify` to compile and get automated feedback on your implementation.

### Build checklist

- [ ] `cmake --build` exits with code `0` and `core_tests` binary appears under `build/tests/core/`
- [ ] `ctest` output ends with `100% tests passed, 0 tests failed`
- [ ] Each test suite is listed individually (e.g. `DeckTest.Has52Cards ... Passed`)
- [ ] No `FAILED` lines anywhere in the output

### Concept checklist

- [ ] Did I use `EXPECT_*` for most assertions and reserve `ASSERT_*` only for null-pointer-dereference risks?
- [ ] Does `test_deck.cpp` verify that the 53rd `deal()` throws `std::out_of_range` using `EXPECT_THROW`?
- [ ] Does `test_deck.cpp` use `std::set` to confirm no duplicates after dealing all 52?
- [ ] Does `test_hand_evaluator.cpp` construct `Card` vectors manually (not via `Deck::deal()`)?
- [ ] Did I cover at least four distinct hand rankings in `test_hand_evaluator.cpp`?
- [ ] Does `MockPlayer::getAction()` return the stored action without touching any real game logic?
- [ ] In `test_game_engine.cpp`, does the fold test assert the folded player is in `foldedPlayers`?
- [ ] Are tests split into separate `.cpp` files per class?

### Common mistakes

| Mistake | Symptom | Fix |
|---|---|---|
| Forgetting `gtest_discover_tests` | Build succeeds but `ctest` reports "No tests found" | Add `gtest_discover_tests(core_tests)` after `target_link_libraries` |
| Using `ASSERT_EQ` inside loops | One mismatch aborts the test, hiding later failures | Use `EXPECT_EQ` inside loops |
| `MockPlayer` missing a virtual override | Linker error or pure virtual call at runtime | Override all three pure virtual methods: `getId`, `dealHoleCards`, `getAction` |
| Building hands via `Deck::deal()` in evaluator tests | Test is fragile — depends on shuffle order | Build `std::vector<Card>` directly with specific `Rank` and `Suit` values |
| `add_subdirectory(core)` still commented out | `core_tests` target never defined | Uncomment in `tests/CMakeLists.txt` |
| Declaring variables in switch cases without `{}` | Compile error: jump bypasses initialization | Wrap each `case` body in `{}` |
