# Phase 2 — Core Layer: Types, Card, Deck, Hand

**Concepts you'll learn:** Value types in C++, enums vs enum class, operator overloading, `std::mt19937` for randomness, header-only vs split header/source.

**Previous phase:** [Phase 1 — CMake Scaffold](phase-1-cmake-scaffold.md)
**Next phase:** [Phase 3 — GameState & GameEngine](phase-3-gamestate-gameengine.md)

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
