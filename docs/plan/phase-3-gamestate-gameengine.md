# Phase 3 — Core Layer: GameState & GameEngine

**Concepts you'll learn:** State machines, STL containers (`std::map`, `std::vector`, `std::set`), const-correctness, threading primitives (`std::mutex`).

**Previous phase:** [Phase 2 — Core Types, Card, Deck, Hand](phase-2-core-types-card-deck-hand.md)
**Next phase:** [Phase 4 — Core Tests with Catch2](phase-4-core-tests.md)

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

- [ ] Create a minimal stub `src/players/IPlayer.hpp` first. A bare forward declaration is NOT enough — `std::unique_ptr<IPlayer>` needs the complete type to call the destructor. The stub needs at minimum:

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

- [ ] Create `src/core/GameEngine.hpp`:

```cpp
#pragma once
#include "GameState.hpp"
#include "../players/IPlayer.hpp"
#include <vector>
#include <memory>
#include <mutex>

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

    // Internal use — const ref, no locking
    const GameState& getState() const;

    // Thread-safe copy for the renderer (main thread reads, engine thread writes)
    GameState getStateSnapshot();

    bool isGameOver() const;

private:
    void startNewHand();
    void runBettingRound();
    void advanceStreet();
    void determineWinner();

    void rotateDealerButton();
    void postBlinds();
    std::vector<PlayerId> buildActionOrder() const;

    GameState  m_state;
    GameConfig m_config;
    std::vector<std::unique_ptr<IPlayer>> m_players;
    Deck       m_deck;

    mutable std::mutex m_stateMutex;  // protects m_state for snapshot reads
};

} // namespace poker
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
