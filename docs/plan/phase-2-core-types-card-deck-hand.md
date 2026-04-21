# Phase 2 — Core Layer: Types, Card, Deck, Hand

**Concepts you'll learn:** Value types, `enum class`, operator overloading, `const` correctness, `std::vector`, `std::mt19937`, exception safety, header vs source split, aggregate types.

**Previous phase:** [Phase 1 — CMake Scaffold](phase-1-cmake-scaffold.md)
**Next phase:** [Phase 3 — GameState & GameEngine](phase-3-gamestate-gameengine.md)

**Design reminder:** `core/` has **zero** dependencies on `ai/`, `players/`, or `ui/`. It's pure poker logic.

---

### Task 2.1: Define shared types in `Types.hpp`

- [ ] Create `src/core/Types.hpp` with:
  - `using PlayerId = int`
  - `enum class Suit` — four suits
  - `enum class Rank` — Two=2 through Ace
  - `enum class Street` — PreFlop, Flop, Turn, River, Showdown
  - `struct Action` with a nested `enum class Type { Fold, Call, Raise }` and `int amount = 0`

**Note on placement:** `Suit` and `Rank` live in `Types.hpp` rather than `Card.hpp`. Files that only need `Rank` or `Suit` don't have to pull in the full `Card` class.

<details>
<summary>Concepts</summary>

> **Concept: `enum class` vs plain `enum`**
>
> Plain `enum` leaks its enumerators into the enclosing scope and implicitly converts to `int`. This causes real bugs:
>
> ```cpp
> // Plain enum — DANGER
> enum Suit { Hearts, Diamonds, Clubs, Spades };
> enum Rank { Two = 2, Three, Four };
>
> if (Hearts == Two)   // compiles silently — both convert to int
>     dealBadly();     // logic error the compiler will never warn you about
> ```
>
> ```cpp
> // enum class — SAFE
> enum class Suit { Hearts, Diamonds, Clubs, Spades };
> enum class Rank { Two = 2, Three, Four };
>
> if (Suit::Hearts == Rank::Two)  // compile error: no comparison between unrelated enum classes
> ```
>
> `enum class` enforces two things: enumerators must be qualified (`Suit::Hearts`, not `Hearts`), and there is no implicit conversion to `int`. You must write `static_cast<int>(Suit::Hearts)` to get the numeric value — explicit and intentional.

> **Concept: `using` type aliases vs `typedef`**
>
> Both create a name for an existing type. `using` is modern C++11 style and reads left-to-right:
>
> ```cpp
> typedef int PlayerId;     // C-style: reads right-to-left, awkward with function pointers
> using PlayerId = int;     // Modern: reads left-to-right, identical semantics — prefer this
> ```
>
> The difference becomes significant with templates: you can make a `using` template alias; you cannot with `typedef`. Always prefer `using` in new C++ code.

> **Concept: `#pragma once` vs include guards**
>
> Include guards prevent a header being processed twice in one translation unit:
>
> ```cpp
> #ifndef POKER_TYPES_HPP
> #define POKER_TYPES_HPP
> // ... contents ...
> #endif
> ```
>
> `#pragma once` does the same in one line, can't have typo bugs (a mismatched guard name causes subtle errors), and lets compilers skip re-reading the file entirely. It's not technically ISO standard, but GCC, Clang, and MSVC all support it. Use `#pragma once` for every header in this project.

> **Concept: Namespaces and nested types**
>
> Namespaces prevent name collisions between your code and third-party libraries. Everything in this project lives in `namespace poker {}`. Never write `using namespace poker;` in a header — it forces the namespace open for every file that includes yours.
>
> `Action::Type` is a nested `enum class` inside `struct Action` rather than a top-level `ActionType`. Compare:
>
> ```cpp
> // Option A — top-level
> enum class ActionType { Fold, Call, Raise };
> struct Action { ActionType type; int amount = 0; };
> // Usage: ActionType::Fold
>
> // Option B — nested (what we use)
> struct Action {
>     enum class Type { Fold, Call, Raise } type;
>     int amount = 0;
> };
> // Usage: Action::Type::Fold
> ```
>
> Option B groups the enum with its owning struct. `Action::Type::Fold` reads as "the Fold variant of an Action's Type." It also avoids adding a standalone `ActionType` name to the `poker::` namespace — important in larger codebases.

**Questions — think through these before checking answers:**
1. What happens if you write `Suit::Hearts == 0` with an `enum class`? What about with a plain `enum`? Try it mentally, then verify with the compiler.
2. `Action::amount` has a default value of `0` in the struct definition. What C++ feature is that, and why does it mean you don't need a constructor to zero-initialize it?

</details>

<details>
<summary>Answers</summary>

**Reference implementation:**

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

---

**Q1.** With `enum class`, `Suit::Hearts == 0` is a **compile error** — there is no implicit conversion from `enum class` to `int`, so comparing with a bare integer literal doesn't compile. With plain `enum`, `Hearts == 0` compiles silently and evaluates to `true` because `Hearts` is the first enumerator (value `0`). That silent `true` is exactly the kind of accidental comparison `enum class` was designed to prevent.

**Q2.** That is a **default member initializer** (C++11). It bakes the initial value directly into the member declaration, so every object of type `Action` has `amount == 0` right from construction — whether you use aggregate initialization (`Action{Action::Type::Fold}`), a compiler-generated constructor, or any future user-defined constructor that doesn't explicitly set `amount`. You would only need to write a constructor if you needed to *compute* the initial value or enforce an invariant at construction time.

</details>

---

### Task 2.2: Implement `Card`

- [ ] Create `src/core/Card.hpp` with:
  - `explicit Card(Rank rank, Suit suit)` constructor
  - `getRank() const` and `getSuit() const` getters
  - `std::string toString() const` — e.g. `"Ace of Spades"`
  - `operator==`, `operator!=`, `operator<`
  - Private members `m_rank` and `m_suit`

- [ ] Create `src/core/Card.cpp` with the implementations.

Tips:
- Use a `switch` in `toString()` for rank and suit names, then concatenate
- `operator<`: compare rank first with `static_cast<int>()`, then suit as tiebreaker
- `operator!=`: delegate to `operator==` — don't repeat the comparison logic
- `static_cast<int>` is required to compare two `enum class` values numerically; the compiler won't do it automatically

<details>
<summary>Concepts</summary>

> **Concept: Value types and the Rule of Zero**
>
> A *value type* behaves like an `int` — it's small, cheap to copy, has no identity beyond its data, and owns no heap resources. Good value types are copyable, movable, and comparable.
>
> Because `Card` holds only two enums (which are just integers), it satisfies all of these automatically. You do not need to write a copy constructor, copy assignment operator, move constructor, move assignment operator, or destructor. This is the **Rule of Zero**: if your class manages no resources, declare none of the special member functions and the compiler generates correct ones for free.

> **Concept: `const` correctness on methods**
>
> A method marked `const` promises not to modify the object it's called on:
>
> ```cpp
> Rank getRank() const;   // does not change the Card
> ```
>
> This lets you call the method on a `const Card&`. Since `GameState` is passed around as `const` reference everywhere, any method you want to call on a card inside game state *must* be `const`. Without it the compiler refuses. Mark every method `const` that doesn't need to modify member variables — that's most getters and all comparison operators.

> **Concept: Operator overloading — member vs free function**
>
> ```cpp
> // Member: left operand is always *this
> bool operator==(const Card& other) const;
>
> // Free function: explicit left and right operands
> bool operator==(const Card& lhs, const Card& rhs);
> ```
>
> For symmetric operators (`==`, `!=`, `<`) on small value types, member functions are less boilerplate and the intent is clear. For stream output (`operator<<`), always use a free function because the left operand (`std::ostream&`) is not your type.

> **Concept: Implement `operator!=` via `operator==`**
>
> Never duplicate logic. The idiomatic pattern:
>
> ```cpp
> bool operator!=(const Card& other) const {
>     return !(*this == other);
> }
> ```
>
> `*this` dereferences the implicit `this` pointer to get the current object, then delegates to the already-correct `operator==`. One place to maintain the equality logic.

> **Concept: `explicit` constructors**
>
> A single-argument constructor acts as an implicit conversion by default:
>
> ```cpp
> Card c = Rank::Ace;  // compiles silently if Card(Rank) is not explicit!
> ```
>
> Mark constructors `explicit` to prevent silent implicit conversions — it signals: "you must deliberately construct a Card."

> **Concept: Header vs source split**
>
> Split into `.hpp` + `.cpp` when the implementation has real logic (switch statements, loops). Keep in `.hpp` only for trivial one-liners. For `Card`:
> - Class declaration + member variables → `Card.hpp`
> - `toString()`, `operator<`, `operator==` — have real logic → `Card.cpp`
>
> Keeping logic in `Card.cpp` means tweaking a string in `toString()` won't trigger a recompile of every file that includes `Card.hpp`.

**Questions — think through these before checking answers:**
1. Why does `operator<` need `static_cast<int>` to compare two `Rank` values, but a plain `enum` would not? What does that tell you about the tradeoff `enum class` makes?
2. If you removed `const` from `getRank()`, what would break elsewhere as cards get used through `const` references?

</details>

<details>
<summary>Answers</summary>

**Reference implementation:**

```cpp
// Card.hpp
#pragma once
#include "Types.hpp"
#include <string>

namespace poker {

class Card {
public:
    explicit Card(Rank rank, Suit suit);

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

```cpp
// Card.cpp
#include "Card.hpp"

namespace poker {

Card::Card(Rank rank, Suit suit) : m_rank(rank), m_suit(suit) {}

Rank Card::getRank() const { return m_rank; }
Suit Card::getSuit() const { return m_suit; }

std::string Card::toString() const {
    std::string rank;
    switch (m_rank) {
        case Rank::Two:   rank = "Two";   break;
        case Rank::Three: rank = "Three"; break;
        case Rank::Four:  rank = "Four";  break;
        case Rank::Five:  rank = "Five";  break;
        case Rank::Six:   rank = "Six";   break;
        case Rank::Seven: rank = "Seven"; break;
        case Rank::Eight: rank = "Eight"; break;
        case Rank::Nine:  rank = "Nine";  break;
        case Rank::Ten:   rank = "Ten";   break;
        case Rank::Jack:  rank = "Jack";  break;
        case Rank::Queen: rank = "Queen"; break;
        case Rank::King:  rank = "King";  break;
        case Rank::Ace:   rank = "Ace";   break;
    }
    std::string suit;
    switch (m_suit) {
        case Suit::Hearts:   suit = "Hearts";   break;
        case Suit::Diamonds: suit = "Diamonds"; break;
        case Suit::Clubs:    suit = "Clubs";    break;
        case Suit::Spades:   suit = "Spades";   break;
    }
    return rank + " of " + suit;
}

bool Card::operator==(const Card& other) const {
    return m_rank == other.m_rank && m_suit == other.m_suit;
}

bool Card::operator!=(const Card& other) const {
    return !(*this == other);
}

bool Card::operator<(const Card& other) const {
    if (m_rank != other.m_rank)
        return static_cast<int>(m_rank) < static_cast<int>(other.m_rank);
    return static_cast<int>(m_suit) < static_cast<int>(other.m_suit);
}

} // namespace poker
```

---

**Q1.** `enum class` deliberately removes the implicit conversion to `int` — that is the whole point of its safety guarantee. Without it, `rank1 < rank2` would silently compare their underlying integers, but it would also allow `Suit::Hearts < Rank::Two`, which is a nonsensical comparison the compiler would never catch. The `static_cast<int>` is the "I know what I'm doing" signal: you are intentionally treating the enum as a number. The tradeoff is explicit: `enum class` prevents accidental cross-type comparisons at the cost of requiring `static_cast` when you genuinely want numeric ordering.

**Q2.** Without `const`, calling `getRank()` on a `const Card&` is a compile error. The compiler message is something like: *"passing `const poker::Card` as `this` argument discards qualifiers."* Concretely: `GameState` passes cards around via `const` references (the renderer reads state but must not modify it), so every place that iterates over cards and calls `getRank()` or `getSuit()` would fail to compile. The fix is always to add `const` back — not to remove `const` from the references.

</details>

---

### Task 2.3: Add `src/core/CMakeLists.txt` and wire up the core library

- [ ] Create `src/core/CMakeLists.txt` defining a `poker_core` static library that:
  - lists `Card.cpp` as a source (more files added per phase)
  - exposes `${CMAKE_SOURCE_DIR}/src` as a `PUBLIC` include directory
  - links `poker_hand_evaluator` as a `PRIVATE` dependency

- [ ] Update `src/CMakeLists.txt` to uncomment `add_subdirectory(core)`

- [ ] Build to verify no errors:
```bash
cmake --build build -j$(nproc)
```

<details>
<summary>Concepts</summary>

> **Concept: CMake target naming**
>
> CMake targets live in a global namespace shared across your entire build — including every `FetchContent` dependency. Naming your target `core` risks silently shadowing a target from SFML, Boost, or any other library. The convention for this project is to prefix all internal targets with `poker_`: `poker_core`, `poker_hand_evaluator`, etc. That makes the intent clear and eliminates collision risk.

> **Concept: `PUBLIC` vs `PRIVATE` in `target_link_libraries`**
>
> The rule is: **who needs to see it?**
> - `PRIVATE` — only this target's `.cpp` files need it. Consumers don't need to `#include` anything from this dependency.
> - `PUBLIC` — your public headers mention types from this dependency. Anyone linking against you must also find it.
> - `INTERFACE` — only consumers need it; your own sources don't (mostly for header-only libs).
>
> **Concrete example of getting it wrong:**
>
> Suppose `GameState.hpp` (a public header of `poker_core`) includes a type from `poker_hand_evaluator`. If you declare it `PRIVATE`:
>
> ```cmake
> target_link_libraries(poker_core PRIVATE poker_hand_evaluator)  # wrong in this scenario
> ```
>
> When `poker_players` links `poker_core` and includes `GameState.hpp`, the compiler sees types from `poker_hand_evaluator` but has no path to its headers — cascade of "unknown type" errors in a file you never touched. Changing to `PUBLIC` fixes this: CMake propagates the include paths to all downstream targets automatically.
>
> In our case `poker_hand_evaluator` types appear only inside `.cpp` files, not in any public header — so `PRIVATE` is correct.

> **Concept: `${CMAKE_SOURCE_DIR}/src` vs `${CMAKE_CURRENT_SOURCE_DIR}`**
>
> `CMAKE_CURRENT_SOURCE_DIR` is the directory of the `CMakeLists.txt` being processed (`src/core/`). `CMAKE_SOURCE_DIR` is always the project root.
>
> All `#include` paths in this project are written relative to `src/`:
> ```cpp
> #include "core/Types.hpp"
> #include "players/IPlayer.hpp"
> ```
>
> If you used `${CMAKE_CURRENT_SOURCE_DIR}`, the root would be `src/core/`, and `#include "core/Types.hpp"` would look for `src/core/core/Types.hpp` — wrong. Using `${CMAKE_SOURCE_DIR}/src` as the single include root keeps paths consistent across every layer.

**Questions — think through these before checking answers:**
1. If `Deck.hpp` (a public header) exposed a type defined inside `poker_hand_evaluator`, would you change `PRIVATE` to `PUBLIC`? Why?
2. When `poker_players` later calls `target_link_libraries(poker_players PRIVATE poker_core)`, what does `poker_players` automatically get without a separate `target_include_directories` call, and why?

</details>

<details>
<summary>Answers</summary>

**Reference implementation:**

```cmake
# src/core/CMakeLists.txt
add_library(poker_core STATIC
    Card.cpp
    # more files added here as we create them
)

target_include_directories(poker_core PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(poker_core PRIVATE
    poker_hand_evaluator
)
```

---

**Q1.** Yes — change it to `PUBLIC`. The rule is: if a dependency's types or headers appear in your *public* headers (the ones consumers `#include`), that dependency must be `PUBLIC` so CMake propagates its include paths to downstream targets. If `Deck.hpp` contained `#include <phevaluator/phevaluator.h>` and used a `phevaluator::Card` in its API, then anyone including `Deck.hpp` would need the evaluator's include path too. Without `PUBLIC`, they'd get "unknown type" errors in files they never touched. Since our `poker_hand_evaluator` usage is confined to `.cpp` files, `PRIVATE` is correct — the dependency is an implementation detail.

**Q2.** `poker_players` automatically gets the include directory `${CMAKE_SOURCE_DIR}/src` — the one that `poker_core` declared as `PUBLIC`. CMake propagates `PUBLIC` include directories (and compile definitions) transitively to all downstream consumers. So `poker_players` can write `#include "core/GameState.hpp"` or `#include "players/IPlayer.hpp"` without any extra `target_include_directories` call, because it inherited the include root from `poker_core`.

</details>

---

### Task 2.4: Implement `Deck`

- [ ] Create `src/core/Deck.hpp` with:
  - Private `std::vector<Card> m_cards` and `std::mt19937 m_rng`
  - Private `void buildDeck()` helper
  - Public `Deck()`, `reset()`, `shuffle()`, `deal()`, `remaining() const`

- [ ] Create `src/core/Deck.cpp` implementing each method:
  - Constructor: call `buildDeck()`, seed `m_rng` with `std::random_device{}()`
  - `buildDeck()`: iterate `int` from `2` to `14` for Rank and `0` to `3` for Suit, cast with `static_cast<Rank>(i)` / `static_cast<Suit>(i)`
  - `deal()`: check `m_cards.empty()`, throw `std::runtime_error` if true, otherwise `back()` + `pop_back()`
  - `reset()`: clear `m_cards` and call `buildDeck()`

<details>
<summary>Concepts</summary>

> **Concept: `std::vector<T>` internals**
>
> `std::vector` stores elements in a single contiguous heap block (like a C array, but managed). Key characteristics:
> - `push_back`: O(1) **amortized** — when capacity is exhausted the vector allocates ~2x the space, copies everything over, and frees the old block. Rare but expensive when it happens; cheap on average.
> - `pop_back`: O(1) always — just decrements the size counter.
> - `back()`: O(1) — reads the last element by index.
>
> Compare to `std::array<Card, 52>`: its size is fixed at **compile time** and baked into the type. You can't write a `reset()` that rebuilds the deck at runtime without knowing the exact size statically. `std::vector` gives dynamic size while staying cache-friendly.

> **Concept: `std::mt19937` and proper seeding**
>
> `rand()` from C is a linear congruential generator with poor statistical properties — it fails many randomness tests and is not suitable for shuffling. `std::mt19937` (Mersenne Twister, period 2^19937 − 1) is the standard C++ choice for high-quality pseudo-randomness.
>
> Seeding matters: a fixed seed gives the same shuffle every run (useful for testing, bad for real play). For a live game, seed from `std::random_device`, which reads entropy from the OS:
>
> ```cpp
> std::mt19937 rng(std::random_device{}());
> ```
>
> `{}()` constructs a temporary `random_device` and immediately calls `operator()` on it for one seed value.

> **Concept: `std::shuffle` and Fisher-Yates**
>
> `std::shuffle` takes a pair of iterators and a random engine, shuffling the range in-place using the Fisher-Yates algorithm: for each position `i` from the last down to 1, pick a random `j` in `[0, i]` and swap. This produces a uniformly random permutation in O(n) time.
>
> ```cpp
> std::shuffle(m_cards.begin(), m_cards.end(), m_rng);
> ```

> **Concept: Exception safety — throw vs sentinel values**
>
> When `deal()` is called on an empty deck you have two options: return a sentinel (a special invalid `Card`, or `std::optional<Card>`) or throw. The sentinel approach has a hidden cost: **every caller must check**, and if one forgets, the bug is silent. An exception is impossible to silently ignore — it unwinds the stack immediately. Calling `deal()` on an empty deck is a logic bug, not a recoverable condition. Prefer `std::runtime_error` or `std::out_of_range` for programming errors like this.

> **Concept: `back()` + `pop_back()` vs `front()` + `erase(begin())`**
>
> Both remove one card from the deck, but performance differs:
> - `back()` + `pop_back()`: **O(1)** — read last element, decrement size.
> - `front()` + `erase(begin())`: **O(n)** — erasing from the front shifts every remaining element left.
>
> Since a shuffled deck has no meaningful "top" vs "bottom", always deal from the back.

**Questions — think through these before checking answers:**
1. Why is `m_rng` stored as a member rather than constructed inside `shuffle()` each call? What would happen to shuffle quality if you re-seeded from `random_device` every time?
2. If you wanted `Deck` to be testable with a deterministic shuffle (same order every run), what one-line change to the constructor signature would allow that?

</details>

<details>
<summary>Answers</summary>

**Reference implementation:**

```cpp
// Deck.hpp
#pragma once
#include "Card.hpp"
#include <vector>
#include <random>

namespace poker {

class Deck {
public:
    Deck();

    void reset();
    void shuffle();
    Card deal();
    int remaining() const;

private:
    std::vector<Card> m_cards;
    std::mt19937      m_rng;

    void buildDeck();
};

} // namespace poker
```

```cpp
// Deck.cpp
#include "Deck.hpp"
#include <stdexcept>
#include <algorithm>

namespace poker {

Deck::Deck() {
    buildDeck();
    m_rng.seed(std::random_device{}());
}

void Deck::buildDeck() {
    m_cards.clear();
    for (int s = 0; s <= 3; ++s)
        for (int r = 2; r <= 14; ++r)
            m_cards.emplace_back(static_cast<Rank>(r), static_cast<Suit>(s));
}

void Deck::reset() {
    m_cards.clear();
    buildDeck();
}

void Deck::shuffle() {
    std::shuffle(m_cards.begin(), m_cards.end(), m_rng);
}

Card Deck::deal() {
    if (m_cards.empty())
        throw std::runtime_error("Deck is empty");
    Card top = m_cards.back();
    m_cards.pop_back();
    return top;
}

int Deck::remaining() const {
    return static_cast<int>(m_cards.size());
}

} // namespace poker
```

---

**Q1.** Re-seeding on every `shuffle()` call wastes the main benefit of `mt19937`: its enormous period (2^19937 − 1) means it produces an astronomically long sequence of high-quality pseudo-random numbers from a single starting point. Resetting the internal state on every call throws that away — you get a brand new sequence each time but only ever use a tiny portion of it. There is also a practical problem: `std::random_device` can be slow (it reads from `/dev/urandom` or similar) and may have limited entropy bandwidth on some systems. Calling it repeatedly in a hot path (a game loop) is wasteful. Store the engine once, seed it once, let it run.

**Q2.** Add a seed parameter with a default:
```cpp
explicit Deck(unsigned int seed = std::random_device{}());
```
Production code calls `Deck deck;` — gets OS entropy. Test code calls `Deck deck(42);` — gets the same shuffle every run, making tests deterministic and reproducible.

</details>

---

### Task 2.5: Implement `Hand`

- [ ] Create `src/core/Hand.hpp` (header-only) — a `struct` with two `Card` members and a constructor using a member initializer list

- [ ] Add `Deck.cpp` to the source list in `src/core/CMakeLists.txt`

- [ ] Build:
```bash
cmake --build build -j$(nproc)
```

- [ ] Commit:
```bash
git add src/core/Types.hpp
git add src/core/Card.hpp src/core/Card.cpp
git add src/core/CMakeLists.txt
git add src/core/Deck.hpp src/core/Deck.cpp
git add src/core/Hand.hpp
git commit -m "feat(core): add Types, Card, Deck, Hand"
```

<details>
<summary>Concepts</summary>

> **Concept: Aggregate types and the `struct` vs `class` convention**
>
> A C++ **aggregate** has no user-declared constructors, no private members, no base classes, and no virtual functions. Aggregates support **aggregate initialization** — you can initialize them with a brace list matching the member order: `Hand{card1, card2}` — without writing a constructor.
>
> The idiomatic C++ convention: **use `struct` for passive data holders, `class` for types with encapsulated behavior and invariants.** `struct` and `class` are nearly identical (the only difference is default access: `public` for `struct`, `private` for `class`), but the choice signals intent. `Hand` is just two cards held together — no invariants to enforce, no behavior to encapsulate. `struct` is the right signal.

> **Concept: Member initializer lists**
>
> The `: first(first), second(second)` syntax before the constructor body is a **member initializer list**. It initializes members directly, rather than default-constructing them first and then assigning inside the body:
>
> ```cpp
> // With initializer list — correct
> Hand(Card f, Card s) : first(f), second(s) {}
>
> // Without — won't compile if Card has no default constructor
> Hand(Card f, Card s) { first = f; second = s; }
> ```
>
> The second form tries to default-construct `first` and `second` before the body runs. Since `Card` has no default constructor, the compiler errors. The initializer list sidesteps this entirely. Always use initializer lists for member initialization.

> **Concept: When header-only is appropriate**
>
> Splitting into `.hpp` + `.cpp` is the default, but adds boilerplate for trivial types. A type is a good candidate for header-only when it's small, its implementation pulls in no expensive-to-compile dependencies, and it has no real logic to hide. `Hand` qualifies on all counts. If it later grows methods with real implementations, split it then.

**Questions — think through these before checking answers:**
1. `Hand` has a constructor, which technically disqualifies it from being a C++ aggregate. Does that matter here? What would you gain or lose by removing the constructor and relying on aggregate initialization (`Hand{card1, card2}`) instead?
2. If `Hand` later needed to validate that `first != second`, where would you put that check — in the constructor body, or somewhere else? Why?

</details>

<details>
<summary>Answers</summary>

**Reference implementation:**

```cpp
// Hand.hpp
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

```cmake
# src/core/CMakeLists.txt — updated source list
add_library(poker_core STATIC
    Card.cpp
    Deck.cpp
)
```

---

**Q1.** It doesn't matter in practice here — the behavior is identical. Both `Hand(card1, card2)` and `Hand{card1, card2}` work and produce the same object. What you would *gain* by removing the constructor: `Hand` becomes a true aggregate, enabling C++20 designated initializers (`Hand{.first = c1, .second = c2}`) which are clearer for readers. What you would *lose*: the named constructor makes it obvious that argument order matters (first card, then second card); with aggregate initialization `Hand{c2, c1}` silently swaps them. For this simple two-member struct either approach is fine — the constructor is a mild preference for explicitness and serves as documentation of intent.

**Q2.** In the constructor body. The constructor is the single correct place for invariant checks because it is the only point where you can prevent an invalid object from ever being fully constructed — a throwing constructor means the object never comes into existence with bad state. If you put the check in a `validate()` method or a free function, callers can forget to call it and the invalid `Hand` lives on silently. This is a core C++ principle: **establish class invariants at construction time** so that any successfully constructed object is guaranteed to be valid.

</details>

---

## Evaluation

Run `/phase-verify` to compile and get automated feedback on your implementation.

### Build checklist

| Step | Command | Success looks like |
|---|---|---|
| Build | `cmake --build build -j$(nproc)` | `[100%]` with no errors; `libpoker_core.a` appears in `build/src/core/` |
| Verify Card compiles | `cmake --build build --target poker_core` | Builds without "undefined reference" or "incomplete type" errors |
| Smoke run | `./build/texas-holdem` | Still exits 0 — main.cpp compiles and links against `poker_core` |

If you added the Deck and Card `.cpp` files to `src/core/CMakeLists.txt`, all three source files should appear in the build output lines.

### Concept checklist

Answer these honestly about your own code:

- [ ] Did I use `enum class` (not plain `enum`) for `Suit`, `Rank`, and `Street`?
- [ ] Did I use `static_cast<int>()` in `Card::operator<` to compare two `enum class` values numerically?
- [ ] Do all getter methods (`getRank`, `getSuit`, `toString`) have the `const` qualifier after the parameter list?
- [ ] Did I implement `operator!=` by delegating to `operator==` (returning `!(*this == other)`) rather than repeating the comparison logic?
- [ ] Is the `Card` constructor marked `explicit`?
- [ ] Does `Deck::deal()` throw `std::runtime_error` (not return a default/invalid card) when the deck is empty?
- [ ] Is `m_rng` a member variable of `Deck` (not re-constructed inside `shuffle()` each call)?
- [ ] Did I use `back()` + `pop_back()` (O(1)) in `Deck::deal()`, not `front()` + `erase(begin())` (O(n))?
- [ ] Is `Deck.cpp` listed in `src/core/CMakeLists.txt`? (Missing this is the most common build error at this phase.)
- [ ] Does `Hand` use a member initializer list (`: first(f), second(s)`) rather than assignment in the constructor body?

### Common mistakes

| Mistake | Symptom | Fix |
|---|---|---|
| Forgetting `static_cast<int>()` in `operator<` | Compiler error: "no match for operator< (operand types are poker::Rank and poker::Rank)" | Add `static_cast<int>()` around both operands: `static_cast<int>(m_rank) < static_cast<int>(other.m_rank)` |
| Not adding `Deck.cpp` (or `Card.cpp`) to `src/core/CMakeLists.txt` | Build succeeds but binary may be incomplete, or "undefined reference to poker::Deck::deal()" at link time | Add the `.cpp` filename to the `add_library(poker_core STATIC ...)` source list |
| Using `Two = 0` in `Rank` enum instead of `Two = 2` | `Deck::buildDeck()` loop produces wrong cards (e.g. `static_cast<Rank>(2)` maps to `Four` instead of `Two`) | Keep `Two = 2` as specified; adjust the `buildDeck()` loop bounds to `i = 2` through `i = 14` |
| Missing `#include <stdexcept>` in `Deck.cpp` before throwing | Compiler error: "'runtime_error' is not a member of 'std'" | Add `#include <stdexcept>` at the top of `Deck.cpp` |
| `using namespace std` in a header (`Card.hpp`, `Types.hpp`, etc.) | Every file that includes that header silently gets the entire `std` namespace — leads to ambiguous name errors that are hard to trace | Remove it; write `std::string`, `std::vector`, etc. explicitly in headers |

### Self-score

- **Solid**: built first try, `poker_core` target compiles clean, answered all checklist questions confidently, understand why `enum class` forces `static_cast`.
- **Learning**: built after 1-2 fixes (missing `static_cast`, forgot to register a `.cpp` in CMakeLists, missing `#include`), concepts are clear after re-reading the concept boxes.
- **Needs review**: multiple compiler errors, confusion about `const` correctness or why `operator<` won't compile; re-read the concept boxes for Task 2.2 and Task 2.4 before moving to Phase 3.
