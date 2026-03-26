# Phase 3 — Core Layer: GameState & GameEngine

**Concepts you'll learn:** State machines, `std::map`, `std::set`, `std::mutex`, `mutable`, thread-safe snapshots, wrapping C libraries, polymorphism via `unique_ptr`, the re-open-on-raise betting rule.

**Previous phase:** [Phase 2 — Core Types, Card, Deck, Hand](phase-2-core-types-card-deck-hand.md)
**Next phase:** [Phase 4 — Core Tests with GoogleTest](phase-4-core-tests.md)

**Design reminder:** `core/` has **zero** dependencies on `ai/`, `players/`, or `ui/`. It's pure poker logic.

---

### Task 3.1: Implement `GameState`

`GameState` is the single source of truth for everything that is happening in the game at any moment: whose turn it is, what cards are on the table, how many chips each player has, who has folded. It flows through the entire system — `GameEngine` writes to it after every action, `GameRenderer` reads it to draw the table, and `IPlayer::getAction()` receives a `const` reference to it so players can make decisions.

The key design decision is choosing the *right container type* for each field. Chip counts are player-keyed data → `std::map`. Folded players are a membership set → `std::set`. Action order is a sequence → `std::vector`. Getting these right now makes every consumer of `GameState` cleaner and less error-prone.

- [x] Create `src/core/GameState.hpp`. Use `docs/spec/design.md` → "core/GameState.hpp" as your field list.

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

<details>
<summary>Concepts</summary>

> **Concept: `std::map<PlayerId, T>` vs `std::vector<T>` for player data**
>
> You might be tempted to write `int chipCounts[8]` or `std::vector<int> chipCounts` indexed by seat. It works — until a player is eliminated or leaves mid-game: now every index shifts and code that reads `chipCounts[2]` suddenly refers to the wrong player. `std::map<PlayerId, int>` makes the player identity explicit:
>
> ```cpp
> // Vector: fragile — index is implicit identity
> int chips = chipCounts[2];          // which player is seat 2?
>
> // Map: self-documenting, stable under removal
> int chips = chipCounts[playerId];   // unambiguous
> ```
>
> The map has slightly higher overhead than a flat array (a red-black tree under the hood), but with ≤9 players the difference is unmeasurable. Clarity wins.

> **Concept: `std::set<PlayerId>` for membership**
>
> `std::set` is the right tool when the operation you need most is "is this ID a member?" — `foldedPlayers.count(id) > 0` — not iteration. Compare the alternatives:
>
> ```cpp
> // Option A: bool array
> bool folded[8] = {};
> folded[2] = true;
> if (folded[2]) { ... }          // fast but fragile — back to index-as-identity
>
> // Option B: vector<PlayerId>
> std::vector<PlayerId> folded;
> folded.push_back(2);
> if (std::find(folded.begin(), folded.end(), 2) != folded.end()) { ... }  // O(n) every time
>
> // Option C: set<PlayerId> — what we use
> std::set<PlayerId> foldedPlayers;
> foldedPlayers.insert(2);
> if (foldedPlayers.count(2) > 0) { ... }  // O(log n), intent is crystal clear
> ```
>
> `std::set` also enables set operations like intersection and difference for free, useful if you ever need "active players" = all players minus folded players.

> **Concept: `struct` with default member initializers**
>
> `GameState` is a `struct` (not a `class`) because it is a plain data container — no invariants to enforce, no behavior to encapsulate, all fields public. In C++11 and later you can give struct members default values directly in the declaration:
>
> ```cpp
> struct GameState {
>     int pot      = 0;       // always starts at zero
>     int minRaise = 0;       // set at start of each hand
>     Street street = Street::PreFlop;   // default to first street
> };
> ```
>
> This means `GameState{}` (default construction) gives you a valid zero-state without writing a constructor. The alternative — leaving members uninitialized — is a source of undefined behaviour that produces subtle, hard-to-reproduce bugs.

> **Concept: `std::vector<PlayerId>` for ordered sequences**
>
> `actionOrder` is a `vector`, not a `set`, because **order matters and duplicates are theoretically possible** (though not in practice for us). The action order is a sequence of player IDs arranged clockwise starting from the first player to act for this street. Iterating through it in order is the main operation. A `set` would discard ordering information; a `vector` preserves it.

**Questions — think through these before checking answers:**
1. Why does `holeCards` need to be `std::map<PlayerId, Hand>` instead of a single `Hand` field? What does the map key represent?
2. `actionOrder` is `std::vector<PlayerId>` while `foldedPlayers` is `std::set<PlayerId>`. What property of each container matches the way each field is *used* in the game?

</details>

<details>
<summary>Answers</summary>

**Reference implementation:**

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
    // Cards
    std::vector<Card>        communityCards;     // 0–5 cards depending on street
    std::map<PlayerId, Hand> holeCards;          // each player's private hole cards

    // Positions
    PlayerId              dealerButton   = 0;
    PlayerId              smallBlindSeat = 0;
    PlayerId              bigBlindSeat   = 0;
    std::vector<PlayerId> actionOrder;           // clockwise from first to act this street

    // Betting
    std::map<PlayerId, int> chipCounts;          // current stack per player
    std::map<PlayerId, int> currentBets;         // amount bet THIS street per player
    int                     pot      = 0;
    int                     minRaise = 0;        // minimum legal raise amount this street

    // State
    Street             street       = Street::PreFlop;
    PlayerId           activePlayer = 0;
    std::set<PlayerId> foldedPlayers;

    // Side pot tracking: total chips put in this hand per player (all streets)
    std::map<PlayerId, int> totalContributed;
};

} // namespace poker
```

---

**Q1.** Multiple players each have their own private two-card holding. A single `Hand` field would represent only one player's cards — you'd need to loop over all players and there'd be no way to associate a hand with its owner. The map key (`PlayerId`) *is* the identity: `holeCards[myId]` retrieves exactly my hand. The engine writes every player's hand into the map on deal; `IPlayer::getAction` reads only its own entry.

**Q2.** `actionOrder` is a vector because you iterate through it in a specific sequence — the order represents who acts when. The same player could in principle appear multiple times after re-opens (though our implementation rebuilds the order instead). `foldedPlayers` is a set because the primary operation is "has this player folded?" (`count(id) > 0`), not iteration. A set gives O(log n) membership lookup and signals that the container holds a *collection of identities*, not an *ordered sequence*.

</details>

---

### Task 3.2: Implement `PlayerView` and `makePlayerView()`

`GameState` holds every player's hole cards in a single map so the engine can deal to everyone and evaluate hands at showdown. But that same map is a liability when the engine passes state to players: `AIPlayer` and `HumanPlayer` receive the full map and could read any opponent's cards. Right now that's prevented only by convention ("don't access `holeCards[otherId]`"), which is a trust-based contract that any future contributor could accidentally break.

`PlayerView` hardens this at the type level. It is a filtered snapshot of `GameState` that replaces the `holeCards` map with a single `myHand` field — only the calling player's two cards. `GameEngine` constructs a `PlayerView` for each player just before asking them to act, so the `IPlayer::getAction()` interface never sees opponent hole cards at all.

This is the standard pattern for information hiding in game engines: the authoritative state (full) lives inside the engine; external actors receive only what they are allowed to see. It also makes `IPlayer::getAction()` easier to reason about — the parameter carries exactly the information a player needs to make a decision, nothing more.

- [x] Create `src/core/PlayerView.hpp` with the `PlayerView` struct and an `inline` `makePlayerView()` factory function.

<details>
<summary>Concepts</summary>

> **Concept: Information hiding at the type level vs by convention**
>
> A "don't do X" rule in a comment is fragile: it relies on every developer reading it, understanding why it matters, and never making a slip. A type-level constraint is enforced by the compiler — you cannot accidentally read `myHand` for the wrong player because there is no map to index into.
>
> This is a core principle of defensive software design: **make illegal states unrepresentable**. Instead of a `GameState` where you *can* read any player's hand but *shouldn't*, give players a `PlayerView` where the opponent hands simply do not exist in the type.

> **Concept: The factory function pattern**
>
> A factory function is a plain function (or static method) whose job is to construct a value type from other data. `makePlayerView(state, forPlayer)` reads the relevant fields from `state` and assembles a `PlayerView`. It centralises the construction logic in one place: if `GameState` gains a new field that belongs in `PlayerView`, there is exactly one line to update.
>
> ```cpp
> // Instead of callers doing:
> PlayerView v;
> v.pot = state.pot;
> v.communityCards = state.communityCards;
> // ... 10 more lines ...
>
> // They just call:
> PlayerView v = makePlayerView(state, myId);
> ```

> **Concept: `inline` for header-defined functions**
>
> Normally, defining a function in a header causes a "multiple definition" linker error if that header is included in more than one `.cpp` file — each translation unit gets its own copy of the definition, and the linker sees duplicates.
>
> `inline` suppresses this: it tells the linker "if you see multiple definitions of this function, they are all identical — keep one." Combined with the compiler's freedom to inline the call site, it is the standard way to define small helper functions in headers.
>
> An alternative is to move the implementation to a `PlayerView.cpp`, which avoids `inline` but adds a compilation unit. For a simple factory with no heavy dependencies, `inline` in the header is the right trade-off.

> **Concept: `std::optional<Hand>` vs asserting the precondition**
>
> `makePlayerView` calls `state.holeCards.at(forPlayer)`, which throws `std::out_of_range` if the player's cards haven't been dealt yet. Two alternatives exist:
>
> - `std::optional<Hand> myHand` — allows `PlayerView` creation before hole cards are dealt; callers must check `myHand.has_value()` before using it.
> - Assert the precondition — document that `makePlayerView` must only be called during a betting round (when hole cards are always present).
>
> We use the second approach. `GameEngine::runBettingRound()` is only called from within `startNewHand()`, after hole cards are dealt — the precondition always holds at call sites inside the engine. `std::optional` would complicate `AIPlayer` and `HumanPlayer` for no practical benefit. If you need to create a `PlayerView` before dealing (e.g., for a spectator view), add a separate `makeSpectatorView()` that omits `myHand` entirely.

**Questions — think through these before checking answers:**
1. `PlayerView` contains `std::map<PlayerId, int> chipCounts` — this shows every player's stack, including opponents. Why is that acceptable to expose while `holeCards` is not?
2. The factory function is `inline` in the header. What would break if you forgot `inline` and included `PlayerView.hpp` in both `GameEngine.cpp` and `AIPlayer.cpp`?

</details>

<details>
<summary>Answers</summary>

**Reference implementation:**

```cpp
// src/core/PlayerView.hpp
#pragma once
#include "GameState.hpp"

namespace poker {

struct PlayerView {
    // Public game information — same as GameState
    std::vector<Card>        communityCards;
    PlayerId                 dealerButton   = 0;
    PlayerId                 smallBlindSeat = 0;
    PlayerId                 bigBlindSeat   = 0;
    std::vector<PlayerId>    actionOrder;
    std::map<PlayerId, int>  chipCounts;
    std::map<PlayerId, int>  currentBets;
    int                      pot      = 0;
    int                      minRaise = 0;
    Street                   street       = Street::PreFlop;
    PlayerId                 activePlayer = 0;
    std::set<PlayerId>       foldedPlayers;

    // Private: only this player's own cards — opponent hands are absent
    PlayerId selfId = 0;
    Hand     myHand;
};

// Precondition: state.holeCards.count(forPlayer) > 0
// (only call during a betting round, after startNewHand() has dealt cards)
inline PlayerView makePlayerView(const GameState& state, PlayerId forPlayer) {
    return PlayerView{
        state.communityCards,
        state.dealerButton,
        state.smallBlindSeat,
        state.bigBlindSeat,
        state.actionOrder,
        state.chipCounts,
        state.currentBets,
        state.pot,
        state.minRaise,
        state.street,
        state.activePlayer,
        state.foldedPlayers,
        forPlayer,
        state.holeCards.at(forPlayer)
    };
}

} // namespace poker
```

---

**Q1.** Chip counts are *public information* in poker. Every player can see every opponent's stack at any time — it is displayed on the table and is essential for making decisions (pot odds, implied odds, all-in sizing). Hole cards are *private information*: each player sees only their own two cards until showdown. Exposing chip counts in `PlayerView` is correct; exposing the hole cards map would be a visibility violation.

**Q2.** Without `inline`, each translation unit that includes `PlayerView.hpp` gets its own definition of `makePlayerView`. At link time the linker sees two symbols with the same name and mangled signature in different object files — "multiple definition of `poker::makePlayerView`" — and fails. `inline` declares: "this function may be defined in multiple translation units; they are all the same definition, merge them." The linker keeps one copy and discards the rest.

</details>

---

### Task 3.3: Understand the state machine before writing `GameEngine`

This is a thinking task, not a coding task. Before writing a single line of `GameEngine`, you need to understand how the game progresses between states. The most common mistake in implementing a game engine is writing the loop first and discovering the exit condition was wrong only after it hangs forever or skips a street.

Texas Hold'em moves through five states — `PreFlop → Flop → Turn → River → Showdown` — and each transition is triggered by the same event: a betting round ending. But "ending" has a precise definition that is easy to get wrong, especially with the re-open-on-raise rule. Writing it out in plain English as a comment forces you to have a correct mental model before the code runs.

- [x] Write answers to the three questions below as a comment block at the top of `GameEngine.hpp` before writing any code — this forces you to understand the transitions before implementing them.

```cpp
// STATE MACHINE TRANSITIONS
// PreFlop → Flop when:  ???
// Flop → Turn when:     ???
// Turn → River when:    ???
// River → Showdown when: ???
// A betting round is "complete" when: ???
```

<details>
<summary>Concepts</summary>

> **Concept: What is a state machine?**
>
> A state machine is a model with a finite set of *states* and *transitions* between them. Each transition is triggered by an event or condition. Our game has states:
>
> ```
> PreFlop → Flop → Turn → River → Showdown → (new hand)
> ```
>
> The event that triggers each transition is "a betting round ends." But *when does a betting round end?* That's the central question of this phase — and the source of most bugs. Sketching the state diagram before writing code prevents the common mistake of writing a betting loop that never terminates or terminates too early.

> **Concept: The re-open-on-raise rule**
>
> This is the trickiest part of the betting round and deserves its own concept box. When the action reaches every non-folded player and all bets are equal, the round ends. But a Raise changes the required bet — now players who already called the previous amount must act again:
>
> ```
> Players: Alice, Bob, Carol. Big blind = 10.
>
> Alice: Call 10.   (Alice acted, all bets equal so far)
> Bob:   Raise 30.  (Bob acted, but now Alice and Carol must respond to 30)
> Carol: Call 30.
> Alice: Call 30.   ← Alice must act again even though she already acted this round
> Bob:   (no action — Bob is the raiser, his bet is already 30)
> → Round ends: all active players acted since the last raise AND all bets are equal.
> ```
>
> The re-open rule: **a Raise resets the "acted" flag for everyone currently in the hand except the raiser.** Implement this with a `needsToAct` set: start it with all active players, remove players as they act, re-add everyone (except the raiser) whenever a raise happens.

> **Concept: Pre-flop vs post-flop action order**
>
> Pre-flop action starts with UTG (Under the Gun — the player to the left of the big blind). This is because the small blind and big blind have already posted forced bets; UTG is the first player with a genuine choice.
>
> Post-flop (Flop, Turn, River) action starts with the first *active* (non-folded) player to the left of the dealer button — typically the small blind if they haven't folded. There is no "UTG" because no forced bets have been posted.

**Questions — think through these before checking answers:**
1. What triggers the transition from PreFlop → Flop?
2. What triggers the transition from River → Showdown?
3. What makes a betting round "complete"?
4. If only one player remains (everyone else folded), do you still need to advance through streets? Why?

</details>

<details>
<summary>Answers</summary>

```
  ╔══════════════════════════════════════════════════════════════╗
  ║  EARLY EXIT — checked after every action in every state     ║
  ║  if only 1 active player remains → award pot → new hand     ║
  ╚══════════════════════════════════════════════════════════════╝

       deal cards, post blinds, UTG acts first
                        │
                        ▼
               ┌─────────────────┐
               │    Pre-Flop     │
               │    (betting)    │
               └────────┬────────┘
                        │  betting round ends
                        ▼
               ┌─────────────────┐
               │      Flop       │
               │   deal 3 cards  │
               │    (betting)    │
               └────────┬────────┘
                        │  betting round ends
                        ▼
               ┌─────────────────┐
               │      Turn       │
               │   deal 1 card   │
               │    (betting)    │
               └────────┬────────┘
                        │  betting round ends
                        ▼
               ┌─────────────────┐
               │     River       │
               │   deal 1 card   │
               │    (betting)    │
               └────────┬────────┘
                        │  betting round ends
                        ▼
               ┌─────────────────┐
               │    Showdown     │
               │  evaluate hands │
               │   award pot     │
               └────────┬────────┘
                        │
                        └──────────► new hand (loop back to top)


  Betting round (zoomed in):

  needsToAct = {all active players}

        ┌──────────────────────────────────────┐
        │  pick next player from needsToAct    │◄─────────────┐
        └──────────────┬───────────────────────┘              │
                       │                                       │
                       ▼                                       │
        ┌──────────────────────────────────────┐              │
        │  ask player: Fold / Call / Raise?    │              │
        └───────┬──────────────┬──────────┬────┘              │
                │              │          │                    │
              Fold           Call       Raise                  │
                │              │          │                    │
                ▼              ▼          ▼                    │
          add to         remove      remove raiser             │
          foldedPlayers  from        from needsToAct;          │
          remove from    needsToAct  re-add ALL other ─────────┘
          needsToAct                 non-folded players

        loop exits when needsToAct is empty
        (or only 1 active player remains → early exit)
```

---

**Q1.** The pre-flop betting round ends. "Ends" means: all active (non-folded) players have acted since the last raise AND all `currentBets` among active players are equal (or a player is all-in and can't match). At that point, deal three community cards (the Flop), reset `currentBets`, rebuild `actionOrder` (now starting left of dealer), and run the next betting round.

**Q2.** The river betting round ends under the same conditions. If two or more players are still active, proceed to Showdown where `HandEvaluator` determines the winner. If only one player remains (everyone folded on the river), award the pot immediately — no showdown needed.

**Q3.** A betting round is complete when: *every non-folded player has acted at least once since the most recent Raise* **AND** *all `currentBets` among non-folded, non-all-in players are equal*. Both conditions must hold simultaneously. The most common mistake is checking only the second condition — that misses the case where everyone limped (called the big blind) and the big blind gets a free option to raise.

**Q4.** No. If only one player is active at any point — even mid-street — the round ends immediately and that player wins the pot. You don't deal community cards for show; you award the pot and start a new hand. This is the "everyone folded" fast path that `runBettingRound()` should check after each action.

</details>

---

### Task 3.4: Define the `GameEngine` interface

`GameEngine` is the heart of the `core/` layer. It owns the authoritative `GameState`, drives the game loop through state transitions, and coordinates all player interactions through the `IPlayer` interface. In the final threading model, it runs on a worker thread — the main thread renders a snapshot of its state every frame.

This task is about the *interface and structure*, not the implementation (that's Task 3.6). You'll define what `GameEngine` looks like from the outside: its constructor, its public methods, and its private data. Two things deserve special attention here. First, `GameEngine` holds a `vector<unique_ptr<IPlayer>>` — this is how C++ achieves polymorphism safely with RAII. Second, `getStateSnapshot()` is designed specifically for the threading model: it returns a *copy* of the state under a mutex lock, so the renderer on the main thread can read it without racing against the engine thread that's actively writing to it.

You also need `src/players/IPlayer.hpp` before `GameEngine` will compile. It needs three methods: `getId()` (so the engine can map players to `PlayerId`s), `dealHoleCards()` (so the engine can deliver cards), and `getAction(const PlayerView&)` (so the engine can ask for an action, passing only what the player is allowed to see). The full interface — `getName()`, comments, etc. — comes in Phase 6.

- [x] Create `src/players/IPlayer.hpp`. Phase 6 adds `getId()`, `getName()`, and other methods — for now, include only what `GameEngine` actually calls. A bare forward declaration is NOT enough: `std::unique_ptr<IPlayer>` needs the complete type to invoke the destructor correctly.

```cpp
#pragma once
#include "core/PlayerView.hpp"
#include "core/Hand.hpp"
#include "core/Types.hpp"

namespace poker {

class IPlayer {
public:
    virtual ~IPlayer() = default;
    // Your turn: add the three pure virtual methods GameEngine calls
    // (getId, dealHoleCards, getAction)
};

} // namespace poker
```

We'll add `getName()` and other methods in Phase 6.

- [x] Create `src/core/GameEngine.hpp`. Paste your state machine comment from Task 3.3 at the top. Declare a `GameConfig` struct (4 int fields) and a `GameEngine` class with:
  - Public: constructor taking `vector<unique_ptr<IPlayer>>` + `GameConfig`; `tick()`, `getStateSnapshot()`, `isGameOver()`
  - Private: `startNewHand()`, `runBettingRound()`, `advanceStreet()`, `determineWinner()`, `rotateDealerButton()`, `postBlinds()`, `buildActionOrder()`
  - Data members: `m_state`, `m_config`, `m_players`, `m_deck`, and a `mutable` mutex for the snapshot

<details>
<summary>Concepts</summary>

> **Concept: `std::unique_ptr` and polymorphism**
>
> You cannot store an abstract type in a `std::vector` directly — the compiler needs to know the exact size of the element type to lay out the contiguous array. `IPlayer` is abstract (has pure virtual methods), so `vector<IPlayer>` is a compile error.
>
> `std::unique_ptr<IPlayer>` solves both problems: the pointer is a fixed size (8 bytes on 64-bit), and it stores a pointer to the concrete type (`HumanPlayer`, `AIPlayer`). The `unique_ptr` calls the virtual destructor when it is destroyed, so the right cleanup code runs even though `GameEngine` only knows the `IPlayer` interface.
>
> ```cpp
> // Won't compile — IPlayer is abstract, size unknown
> std::vector<IPlayer> m_players;
>
> // Works — pointer is always 8 bytes, virtual dispatch handles the rest
> std::vector<std::unique_ptr<IPlayer>> m_players;
> ```
>
> The `unique_ptr` also expresses *ownership*: `GameEngine` owns the players and is responsible for their lifetime. When `GameEngine` is destroyed, all `unique_ptr`s are destroyed, which destroys all players automatically. No `delete` required.

> **Concept: `std::mutex` and data races**
>
> A *data race* happens when two threads access the same memory at the same time and at least one of them writes. The result is undefined behaviour — not just "wrong output," but potential crashes, corrupted memory, or heisenbugs that only appear in production.
>
> In our threading model: the engine thread writes `m_state` every time it applies an action. The main thread reads `m_state` every frame to render it. Without synchronization, these overlap freely — a data race.
>
> `std::mutex` prevents this. Only one thread can hold the mutex at a time. The pattern:
>
> ```cpp
> // Engine thread — writing
> {
>     std::lock_guard<std::mutex> lock(m_stateMutex);
>     m_state.pot += amount;   // safe: no other thread can enter while lock is held
> }  // lock released here (RAII)
>
> // Renderer thread — reading (snapshot)
> GameState snap;
> {
>     std::lock_guard<std::mutex> lock(m_stateMutex);
>     snap = m_state;          // copy made under the lock
> }
> render(snap);               // use the copy — lock already released
> ```
>
> `std::lock_guard` is an RAII wrapper: it locks on construction and unlocks on destruction (when the scope exits). You cannot forget to unlock.

> **Concept: `mutable` — exempting a member from `const`**
>
> `getStateSnapshot()` is a logically read-only operation — it doesn't change the game state. It makes sense to mark it `const`. But locking a mutex *mutates* it. Without `mutable`, the compiler rejects this:
>
> ```
> error: passing 'const std::mutex' as 'this' discards qualifiers
> ```
>
> `mutable` tells the compiler: "this member is a synchronization primitive, not logical state. Allow it to be modified even inside `const` methods."
>
> ```cpp
> mutable std::mutex m_stateMutex;  // OK to lock in const methods
> ```
>
> `mutable` is specifically for two legitimate uses: synchronization primitives (mutex, atomic) and lazy-computed caches. It is not a way to bypass `const` for normal data members — doing so breaks the semantic contract and confuses readers.

> **Concept: Return by value for thread safety**
>
> `getStateSnapshot()` returns `GameState` by value, not `const GameState&`. This is deliberate and important.
>
> If it returned a reference, the main thread would hold a direct pointer into `m_state`. The moment the lock releases, the engine thread is free to modify `m_state` — and does, on every tick. The main thread is now reading data that is being written concurrently: a data race.
>
> Returning by value copies `m_state` under the lock, then returns the copy. After `return`, the engine can modify `m_state` freely because the renderer holds its own independent copy.
>
> ```cpp
> GameState GameEngine::getStateSnapshot() {
>     std::lock_guard<std::mutex> lock(m_stateMutex);
>     return m_state;   // copy made, lock still held; lock releases on exit
> }
> ```
>
> The copy is not free — `GameState` contains several maps and vectors. But compared to the alternative (undefined behaviour from a data race), the cost is trivial.

**Questions — think through these before checking answers:**
1. Why does `GameEngine`'s constructor take `std::vector<std::unique_ptr<IPlayer>>` by value (not by reference or const reference)? Think about what happens to the vector after the constructor returns.
2. Why is `m_stateMutex` declared `mutable`? What would happen if you removed `mutable` and kept `getStateSnapshot()` as `const`?
3. `getStateSnapshot()` returns `GameState` (by value) while the internal `getState()` method returns `const GameState&`. Why the difference?

</details>

<details>
<summary>Answers</summary>

**Reference implementation — `IPlayer.hpp`:**

```cpp
#pragma once
#include "core/PlayerView.hpp"
#include "core/Hand.hpp"
#include "core/Types.hpp"

namespace poker {

class IPlayer {
public:
    virtual ~IPlayer() = default;
    virtual PlayerId getId()                           const = 0;
    virtual void     dealHoleCards(const Hand& cards)       = 0;
    virtual Action   getAction(const PlayerView& view)      = 0;
};

} // namespace poker
```

**Reference implementation — `GameEngine.hpp`:**

```cpp
#pragma once
// STATE MACHINE TRANSITIONS
// PreFlop → Flop when:   pre-flop betting round ends
// Flop → Turn when:      flop betting round ends
// Turn → River when:     turn betting round ends
// River → Showdown when: river betting round ends
// A betting round is "complete" when: all active players have acted since
//   the last raise AND all currentBets among non-folded, non-all-in players
//   are equal (needsToAct set is empty)

#include "GameState.hpp"
#include "Deck.hpp"
#include "../players/IPlayer.hpp"
#include <vector>
#include <memory>
#include <mutex>

namespace poker {

struct GameConfig {
    int numPlayers    = 2;
    int startingStack = 1000;
    int smallBlind    = 5;
    int bigBlind      = 10;
};

class GameEngine {
public:
    GameEngine(std::vector<std::unique_ptr<IPlayer>> players, GameConfig config);

    // Advance the game by one action (ask active player, apply it)
    void tick();

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

**Q1.** The constructor takes the vector *by value* and stores it as `m_players`. If it took a `const vector<unique_ptr<IPlayer>>&`, the constructor couldn't *move* the `unique_ptr`s out of the argument into `m_players` — `const` prevents move. And copying `unique_ptr` is deleted (unique ownership means no copies). The idiomatic pattern: pass by value, then move-assign: `m_players(std::move(players))`. The caller does `GameEngine engine(std::move(myPlayers), config);` — the players are transferred in one O(1) operation, no copies.

**Q2.** Without `mutable`, the compiler emits: *"passing `const std::mutex` as `this` argument discards qualifiers."* The `lock()` call inside `std::lock_guard` is a non-`const` method on `std::mutex`, but `m_stateMutex` is a member of a `const`-qualified object (because `getStateSnapshot()` is `const`). `mutable` exempts the mutex from the const contract. Logically, locking a mutex doesn't change the *state* of the game — it's bookkeeping. `mutable` expresses exactly that intent.

**Q3.** `getStateSnapshot()` is designed for **cross-thread reads** (the renderer on the main thread). It locks, copies, and returns a fully independent `GameState`. After the function returns, the returned copy is safe to use without any locking because it's a separate object. `getState()` (returning `const GameState&`) is for **same-thread reads** inside the engine — for example, a private helper called from within `tick()` that needs to inspect state without the overhead of a copy or a lock it already holds (taking the same mutex twice on the same thread with `lock_guard` is a deadlock).

</details>

---

### Task 3.5: Implement `HandEvaluator` and add `Card::toShortString()`

At showdown, `GameEngine` needs to compare the hands of all remaining players to determine who wins the pot. Evaluating a poker hand from scratch — ranking straights, flushes, full houses, and so on — is a non-trivial algorithm. Rather than implementing it yourself (a worthwhile exercise, but not the focus right now), you'll wrap an existing C library: [HenryRLee/PokerHandEvaluator](https://github.com/HenryRLee/PokerHandEvaluator).

`HandEvaluator` is a thin, type-safe wrapper around this library. It takes a `vector<Card>` (your types) and returns a numeric score (lower = stronger hand). The wrapping pattern — hide the C API behind a clean C++ class — is a skill you'll use throughout your career whenever you work with third-party C libraries (OpenSSL, SQLite, FFmpeg, etc.).

There's one preparatory step before implementing the evaluator: the library's C++ interface constructs cards from short strings like `"Ac"` (Ace of Clubs). Your `Card::toString()` from Phase 2 returns `"Ace of Spades"` — the wrong format. You'll add a `toShortString()` method to `Card` first, then use it inside the evaluator. This keeps the encoding knowledge in `Card` where it belongs, and makes `HandEvaluator.cpp` encoding-agnostic.

The C++ upgrade happens in Task 3.6. For now, you'll implement using the C API to feel the friction of raw integers and manual encoding — which motivates the upgrade.

- [x] Add `std::string toShortString() const` to `Card.hpp` and implement it in `Card.cpp`. Expected output: rank letter (`2`–`9`, `T`, `J`, `Q`, `K`, `A`) + suit letter (`c`, `d`, `h`, `s`). Example: `Card{Rank::Ace, Suit::Spades}.toShortString()` → `"As"`.

- [x] Create `src/core/HandEvaluator.hpp`. Declare a `HandEvaluator` class with two `static` methods: `evaluate()` (takes exactly 7 `Card`s, returns `int` score — lower = stronger) and `beats()` (returns true if the first 7-card hand beats the second).

```cpp
#pragma once
#include "Card.hpp"
#include <vector>

namespace poker {

class HandEvaluator {
public:
    // Your turn: declare static evaluate() and beats()
};

} // namespace poker
```

- [x] Implement `HandEvaluator.cpp` using the C API (Task 3.7 upgrades this to C++). The C API function is `evaluate_7cards(c0, c1, ..., c6)` where each card is an int: `rank * 4 + suit`. Your `Rank` enum starts at `Two=2` but the library expects `Two=0` — you'll need to subtract 2.

- [ ] Add `HandEvaluator.cpp` to `src/core/CMakeLists.txt` and confirm `poker_hand_evaluator` is linked.

<details>
<summary>Concepts</summary>

> **Concept: Static methods for stateless utilities**
>
> `HandEvaluator` has no member variables — it holds no state. Every call to `evaluate()` is purely a function of its inputs. The `static` keyword on a method means it belongs to the class itself, not to any instance. You call it as `HandEvaluator::evaluate(cards)`, not `evaluator.evaluate(cards)`.
>
> For stateless utilities, `static` is preferable over requiring callers to construct an instance just to call a method. The class name serves as a namespace. An alternative is to make them free functions in a namespace, but grouping them in a class keeps the API surface organised and makes it mockable if needed (via a non-static interface later).

> **Concept: Wrapping a C library**
>
> C libraries expose functions with C-style signatures — plain integers, raw arrays, no RAII. Wrapping them in a C++ class gives you:
> - A type-safe boundary: callers pass `Card` objects, not magic integers
> - One place to fix encoding issues (the `- 2` offset, for example)
> - Easy replaceability: swap the implementation without touching callers
>
> The `static int toLibCard(const Card&)` function inside the `.cpp` is an *implementation detail* — not part of the public API. It lives in the anonymous namespace or as a `static` free function so it can't be called from outside the file.

> **Concept: The offset mismatch and why it matters**
>
> Your `Rank` enum starts at `Two = 2` (matching the card face value). The PokerHandEvaluator C API expects `Two = 0`. If you forget to subtract 2:
>
> ```cpp
> // Bug: Rank::Two (int value 2) passed as rank 2 → library reads it as "Four"
> // Bug: Rank::Ace (int value 14) passed as rank 14 → out of range, undefined behavior
> int rank = static_cast<int>(c.getRank());  // WRONG
>
> // Correct: shift the range from [2, 14] to [0, 12]
> int rank = static_cast<int>(c.getRank()) - 2;  // RIGHT
> ```
>
> The C++ `pheval` interface (Task 3.6) eliminates this entirely by accepting a string like `"Ac"` — no manual encoding. Until then, the offset is in one place (`toLibCard`) and easily verified.

> **Concept: The `toShortString()` format**
>
> `phevaluator::Card` is constructed from a two-character string: rank character + suit character. Rank chars: `2`–`9`, `T` (Ten), `J`, `Q`, `K`, `A`. Suit chars: `c` (Clubs), `d` (Diamonds), `h` (Hearts), `s` (Spades). Keep this in `Card.cpp` as a private implementation detail until Task 3.6.

**Questions — think through these before checking answers:**
1. Why is `toLibCard()` defined as a file-local `static` function in `HandEvaluator.cpp` rather than a public method on `Card` or a public static method on `HandEvaluator`?
2. `beats(a, b)` returns `evaluate(a) < evaluate(b)`. In what direction does the library score hands — and what would you need to change in `GameEngine::determineWinner()` if you wanted to find the *winner* among N players using `std::min_element`?

</details>

<details>
<summary>Answers</summary>

**Reference implementation — `Card::toShortString()` addition:**

```cpp
// Card.hpp — add to public interface
std::string toShortString() const;

// Card.cpp — add implementation
std::string Card::toShortString() const {
    std::string rank;
    switch (m_rank) {
        case Rank::Two:   rank = "2"; break;
        case Rank::Three: rank = "3"; break;
        case Rank::Four:  rank = "4"; break;
        case Rank::Five:  rank = "5"; break;
        case Rank::Six:   rank = "6"; break;
        case Rank::Seven: rank = "7"; break;
        case Rank::Eight: rank = "8"; break;
        case Rank::Nine:  rank = "9"; break;
        case Rank::Ten:   rank = "T"; break;
        case Rank::Jack:  rank = "J"; break;
        case Rank::Queen: rank = "Q"; break;
        case Rank::King:  rank = "K"; break;
        case Rank::Ace:   rank = "A"; break;
    }
    std::string suit;
    switch (m_suit) {
        case Suit::Clubs:    suit = "c"; break;
        case Suit::Diamonds: suit = "d"; break;
        case Suit::Hearts:   suit = "h"; break;
        case Suit::Spades:   suit = "s"; break;
    }
    return rank + suit;
}
```

**Reference implementation — `HandEvaluator.hpp`:**

```cpp
#pragma once
#include "Card.hpp"
#include <vector>

namespace poker {

class HandEvaluator {
public:
    // Evaluate best 5-card hand from exactly 7 cards (2 hole + 5 community).
    // Returns a score: lower value = stronger hand (library convention).
    static int evaluate(const std::vector<Card>& sevenCards);

    // Returns true if handA beats handB.
    static bool beats(const std::vector<Card>& handA, const std::vector<Card>& handB);
};

} // namespace poker
```

**Reference implementation — `HandEvaluator.cpp` (C API):**

```cpp
#include "HandEvaluator.hpp"
#include <phevaluator/phevaluator.h>

namespace poker {

// Convert our Card to the C library's integer encoding.
// The C API expects: rank 0=Two..12=Ace, suit 0=Club..3=Spade.
// Our Rank enum starts at Two=2, so we subtract 2 to get 0-based.
static int toLibCard(const Card& c) {
    int rank = static_cast<int>(c.getRank()) - 2;  // Two=2 → 0, Ace=14 → 12
    int suit = static_cast<int>(c.getSuit());       // Club=0, Spade=3
    return rank * 4 + suit;
}

int HandEvaluator::evaluate(const std::vector<Card>& cards) {
    return evaluate_7cards(
        toLibCard(cards[0]), toLibCard(cards[1]),
        toLibCard(cards[2]), toLibCard(cards[3]),
        toLibCard(cards[4]), toLibCard(cards[5]),
        toLibCard(cards[6])
    );
}

bool HandEvaluator::beats(const std::vector<Card>& a, const std::vector<Card>& b) {
    return evaluate(a) < evaluate(b);  // lower score = stronger hand
}

} // namespace poker
```

---

**Q1.** `toLibCard()` is an implementation detail of `HandEvaluator.cpp` that encodes the library's internal integer format. Exposing it publicly would leak the C API's encoding details into the rest of the codebase — callers would need to know about the `- 2` offset, the `rank * 4 + suit` formula, and how the library numbers suits. When Task 3.6 upgrades to the C++ interface, this function disappears entirely. Keeping it file-local (`static` free function in the `.cpp`) means changing or removing it requires only changing one file. The rule: hide details that are likely to change.

**Q2.** Lower score = stronger hand (e.g., a Royal Flush scores 1, the worst possible hand scores 7462). To find the winner among N players using `std::min_element`, compare their `evaluate()` scores — the player with the *lowest* score wins. `std::min_element` already does this correctly with the default `<` comparator. Note: in Task 3.6, `phevaluator::Rank` has its `operator<` reversed (`value_ > other.value_`) so that "better hand is greater," allowing `std::max_element` to find the winner without a custom comparator. Whether you use min or max depends on which interface you're using.

</details>

---

### Task 3.6: Implement `GameEngine.cpp`

This is the most complex implementation task in the project. `GameEngine.cpp` is where all the poker rules live: shuffling and dealing, rotating the dealer button, posting blinds, running betting rounds with the re-open-on-raise rule, dealing community cards across streets, and determining the winner at showdown.

The key to implementing this without getting lost is to go method by method from the bottom up — implement the small helpers (`rotateDealerButton`, `postBlinds`, `buildActionOrder`) before the methods that call them (`startNewHand`, `advanceStreet`). `runBettingRound()` is the hardest method and deserves the most care: the re-open-on-raise rule is subtle and a wrong implementation produces a loop that either never ends or exits too early.

**Before writing `runBettingRound()`**, re-read your answers from Task 3.2 and the betting round algorithm in `docs/spec/design.md`. The `needsToAct` set pattern (explained in the Concepts section) is the cleanest implementation — understand it before writing code.

Take it method by method. **Read the betting round algorithm in `docs/spec/design.md` → "core/GameEngine.hpp" before writing `runBettingRound()`.**

- [ ] Implement `rotateDealerButton()` — advance `dealerButton` to the next player with chips; set `smallBlindSeat` and `bigBlindSeat` accordingly.
- [ ] Implement `postBlinds()` — subtract small and big blind from their respective `chipCounts`, set `currentBets`, add to `pot`, set `minRaise = m_config.bigBlind`.
- [ ] Implement `buildActionOrder()` — return a `vector<PlayerId>` of non-folded, non-broke players in clockwise order starting from UTG (pre-flop) or left of dealer (post-flop).
- [ ] Implement `startNewHand()` — reset/shuffle deck, deal 2 cards per active player, rotate dealer, post blinds, clear community cards, set street to `PreFlop`, build action order, call `runBettingRound()`.
- [ ] Implement `runBettingRound()` — loop using a `needsToAct` set; on Raise, re-add all non-folded players except the raiser; stop when `needsToAct` is empty or only one active player remains.
- [ ] Implement `advanceStreet()` — deal 3 community cards for Flop, 1 for Turn, 1 for River; reset `currentBets`; rebuild `actionOrder`; call `runBettingRound()`.
- [ ] Implement `determineWinner()` — build side pots from `totalContributed` (one pot per all-in level, eligible players = non-folded contributors at that level); for each pot find the best eligible hand using `HandEvaluator::evaluate()`; award each pot to its winner; clear `pot`.
- [ ] Implement `tick()` — call `startNewHand()` if no hand in progress; or advance state depending on current street.
- [ ] Implement `getStateSnapshot()` — lock `m_stateMutex`, return copy of `m_state`.
- [ ] Add `GameEngine.cpp` to `src/core/CMakeLists.txt`. (`PlayerView.hpp` is header-only — no `.cpp` to register.)
- [ ] Build:
```bash
cmake --build build -j$(nproc)
```
- [ ] Commit:
```bash
git add src/core/GameState.hpp
git add src/core/PlayerView.hpp
git add src/core/HandEvaluator.hpp src/core/HandEvaluator.cpp
git add src/core/GameEngine.hpp src/core/GameEngine.cpp
git add src/players/IPlayer.hpp
git add src/core/CMakeLists.txt
git commit -m "feat(core): add GameState, PlayerView, HandEvaluator, GameEngine state machine"
```

<details>
<summary>Concepts</summary>

> **Concept: `startNewHand()` — the reset sequence matters**
>
> Order is important. Reset and shuffle the deck *before* rotating the dealer button, because rotating the button modifies `m_state` fields that `buildActionOrder()` reads. Then:
> 1. `m_state.foldedPlayers.clear()`
> 2. `m_state.currentBets.clear()`  (or zero out entries)
> 3. `m_state.communityCards.clear()`
> 4. Rotate dealer, set blinds
> 5. Post blinds (modifies `chipCounts`, `currentBets`, `pot`)
> 6. Deal hole cards (modifies `holeCards`, calls `player->dealHoleCards()`)
> 7. Set `street = Street::PreFlop`
> 8. Build action order
> 9. Run pre-flop betting round
>
> If you post blinds *after* dealing hole cards, the order still works, but posting blinds *after* building the action order means the `minRaise` isn't set when the first player acts — subtle bug.

> **Concept: The `needsToAct` pattern for betting rounds**
>
> The cleanest implementation of the re-open-on-raise rule:
>
> ```cpp
> void GameEngine::runBettingRound() {
>     // Everyone who isn't folded or all-in needs to act at least once
>     std::set<PlayerId> needsToAct;
>     for (PlayerId id : m_state.actionOrder) {
>         if (m_state.foldedPlayers.count(id) == 0)
>             needsToAct.insert(id);
>     }
>
>     while (!needsToAct.empty()) {
>         // Check for early exit: only 1 player left
>         int activePlayers = /* count non-folded players */ ;
>         if (activePlayers <= 1) break;
>
>         PlayerId id = // next player from actionOrder who is in needsToAct
>         m_state.activePlayer = id;
>         Action action = findPlayer(id)->getAction(m_state);
>
>         if (action.type == Action::Type::Fold) {
>             m_state.foldedPlayers.insert(id);
>             needsToAct.erase(id);
>         } else if (action.type == Action::Type::Call) {
>             applyCall(id);
>             needsToAct.erase(id);
>         } else { // Raise
>             applyRaise(id, action.amount);
>             // Reopen: everyone except the raiser needs to act again
>             needsToAct.clear();
>             for (PlayerId pid : m_state.actionOrder) {
>                 if (pid != id && m_state.foldedPlayers.count(pid) == 0)
>                     needsToAct.insert(pid);
>             }
>         }
>     }
> }
> ```

> **Concept: `determineWinner()` — building the 7-card hand**
>
> `HandEvaluator::evaluate()` needs exactly 7 cards: 2 hole cards + 5 community cards. Build the vector by concatenating:
>
> ```cpp
> std::vector<Card> sevenCards;
> sevenCards.push_back(m_state.holeCards[playerId].first);
> sevenCards.push_back(m_state.holeCards[playerId].second);
> for (const Card& c : m_state.communityCards)
>     sevenCards.push_back(c);
> int score = HandEvaluator::evaluate(sevenCards);
> ```
>
> Do this for each non-folded player, track the one with the lowest score, award them `m_state.pot`.

> **Concept: Finding a player by ID**
>
> `m_players` is a `vector<unique_ptr<IPlayer>>`. To get the `IPlayer*` for a given `PlayerId`, you need a linear search or a parallel map. The simplest approach:
>
> ```cpp
> IPlayer* GameEngine::findPlayer(PlayerId id) const {
>     for (auto& p : m_players)
>         if (p->getId() == id) return p.get();
>     return nullptr;
> }
> ```
>
> With ≤9 players this is O(n) but fast in practice. You could build a `std::map<PlayerId, IPlayer*>` in the constructor if you wanted O(1) lookup — worthwhile only if profiling reveals it's a bottleneck.

**Questions — think through these before checking answers:**
1. Why does `postBlinds()` need to set `m_state.currentBets` for the blind players before `runBettingRound()` starts, rather than having them "act" in the first round like other players?
2. After a player goes all-in (calls for less than the full current bet), should they appear in `needsToAct` on the next raise? Why?

</details>

<details>
<summary>Answers</summary>

**Reference implementation — key methods:**

```cpp
// GameEngine.cpp

#include "GameEngine.hpp"
#include "HandEvaluator.hpp"
#include "PlayerView.hpp"
#include <algorithm>
#include <stdexcept>

namespace poker {

GameEngine::GameEngine(std::vector<std::unique_ptr<IPlayer>> players, GameConfig config)
    : m_players(std::move(players))
    , m_config(config)
{
    // Initialize chip counts for all players
    for (auto& player : m_players) {
        m_state.chipCounts[player->getId()]      = m_config.startingStack;
        m_state.currentBets[player->getId()]     = 0;
        m_state.totalContributed[player->getId()] = 0;
    }
    m_state.dealerButton = 0;
    startNewHand();
}

void GameEngine::rotateDealerButton() {
    int n = static_cast<int>(m_players.size());
    for (int i = 1; i <= n; ++i) {
        PlayerId candidate = (m_state.dealerButton + i) % n;
        if (m_state.chipCounts[candidate] > 0) {
            m_state.dealerButton = candidate;
            break;
        }
    }
    if (n == 2) {
        // Heads-up: dealer posts small blind, other player posts big blind
        m_state.smallBlindSeat = m_state.dealerButton;
        m_state.bigBlindSeat   = (m_state.dealerButton + 1) % n;
    } else {
        m_state.smallBlindSeat = (m_state.dealerButton + 1) % n;
        m_state.bigBlindSeat   = (m_state.dealerButton + 2) % n;
    }
}

void GameEngine::postBlinds() {
    auto deduct = [&](PlayerId id, int amount) {
        int actual = std::min(amount, m_state.chipCounts[id]);
        m_state.chipCounts[id]       -= actual;
        m_state.currentBets[id]      += actual;
        m_state.pot                  += actual;
        m_state.totalContributed[id] += actual;
    };
    deduct(m_state.smallBlindSeat, m_config.smallBlind);
    deduct(m_state.bigBlindSeat,   m_config.bigBlind);
    m_state.minRaise = m_config.bigBlind;
}

std::vector<PlayerId> GameEngine::buildActionOrder() const {
    int n = static_cast<int>(m_players.size());
    int start;
    if (m_state.street == Street::PreFlop)
        start = (m_state.bigBlindSeat + 1) % n;   // UTG
    else
        start = (m_state.dealerButton + 1) % n;   // left of dealer

    std::vector<PlayerId> order;
    for (int i = 0; i < n; ++i) {
        PlayerId id = (start + i) % n;
        if (m_state.foldedPlayers.count(id) == 0 && m_state.chipCounts[id] > 0)
            order.push_back(id);
    }
    return order;
}

void GameEngine::startNewHand() {
    m_state.foldedPlayers.clear();
    m_state.communityCards.clear();
    m_state.pot      = 0;
    m_state.minRaise = m_config.bigBlind;
    for (auto& [id, bet] : m_state.currentBets)       bet = 0;
    for (auto& [id, c]   : m_state.totalContributed)  c   = 0;

    m_deck.reset();
    m_deck.shuffle();

    rotateDealerButton();
    postBlinds();

    // Deal hole cards
    for (auto& player : m_players) {
        PlayerId id = player->getId();
        if (m_state.chipCounts[id] > 0) {
            Hand hand{m_deck.deal(), m_deck.deal()};
            m_state.holeCards[id] = hand;
            player->dealHoleCards(hand);
        }
    }

    m_state.street = Street::PreFlop;
    m_state.actionOrder = buildActionOrder();
    runBettingRound();
    awardPotIfHandOver();
}

void GameEngine::runBettingRound() {
    std::set<PlayerId> needsToAct(
        m_state.actionOrder.begin(), m_state.actionOrder.end());

    // Preserve iteration order from actionOrder
    while (!needsToAct.empty()) {
        // Early exit: only one player left in the hand (includes all-in players)
        int active = 0;
        for (auto& [id, chips] : m_state.chipCounts)
            if (m_state.foldedPlayers.count(id) == 0) ++active;
        if (active <= 1) break;

        // Find next player from actionOrder who still needs to act
        PlayerId actingId = -1;
        for (PlayerId id : m_state.actionOrder) {
            if (needsToAct.count(id) > 0) { actingId = id; break; }
        }
        if (actingId == -1) break;

        m_state.activePlayer = actingId;
        IPlayer* player = nullptr;
        for (auto& p : m_players)
            if (p->getId() == actingId) { player = p.get(); break; }

        // Build a filtered view: player sees only their own hole cards
        PlayerView view = makePlayerView(m_state, actingId);
        Action action = player->getAction(view);

        // Find current max bet
        int maxBet = 0;
        for (auto& [id, bet] : m_state.currentBets) maxBet = std::max(maxBet, bet);

        if (action.type == Action::Type::Fold) {
            m_state.foldedPlayers.insert(actingId);
            needsToAct.erase(actingId);

        } else if (action.type == Action::Type::Call) {
            int callAmt = std::min(maxBet - m_state.currentBets[actingId],
                                   m_state.chipCounts[actingId]);
            m_state.chipCounts[actingId]       -= callAmt;
            m_state.currentBets[actingId]      += callAmt;
            m_state.pot                        += callAmt;
            m_state.totalContributed[actingId] += callAmt;
            needsToAct.erase(actingId);

        } else { // Raise
            int raiseTotal = std::clamp(action.amount,
                                        maxBet + m_state.minRaise,
                                        m_state.chipCounts[actingId] + m_state.currentBets[actingId]);
            int additional = raiseTotal - m_state.currentBets[actingId];
            m_state.minRaise = raiseTotal - maxBet;
            m_state.chipCounts[actingId]       -= additional;
            m_state.currentBets[actingId]       = raiseTotal;
            m_state.pot                        += additional;
            m_state.totalContributed[actingId] += additional;

            // Reopen action to all non-folded, non-all-in players except the raiser
            needsToAct.clear();
            for (PlayerId id : m_state.actionOrder) {
                if (id != actingId && m_state.foldedPlayers.count(id) == 0
                        && m_state.chipCounts[id] > 0)
                    needsToAct.insert(id);
            }
        }
    }
}

void GameEngine::advanceStreet() {
    // Reset bets for the new street
    for (auto& [id, bet] : m_state.currentBets) bet = 0;
    m_state.minRaise = m_config.bigBlind;

    // Deal community cards and advance the street enum.
    // River → Showdown: no cards dealt; call determineWinner() and return early
    // so we never call runBettingRound() at Showdown.
    if (m_state.street == Street::PreFlop) {
        for (int i = 0; i < 3; ++i) m_state.communityCards.push_back(m_deck.deal());
        m_state.street = Street::Flop;
    } else if (m_state.street == Street::Flop) {
        m_state.communityCards.push_back(m_deck.deal());
        m_state.street = Street::Turn;
    } else if (m_state.street == Street::Turn) {
        m_state.communityCards.push_back(m_deck.deal());
        m_state.street = Street::River;
    } else if (m_state.street == Street::River) {
        m_state.street = Street::Showdown;
        determineWinner();
        return;  // no betting at showdown
    }

    m_state.actionOrder = buildActionOrder();
    runBettingRound();
    awardPotIfHandOver();
}

bool GameEngine::awardPotIfHandOver() {
    int active = 0;
    for (auto& [id, chips] : m_state.chipCounts)
        if (m_state.foldedPlayers.count(id) == 0) ++active;  // count all-in players too
    if (active <= 1) {
        determineWinner();
        m_state.street = Street::Showdown;
        return true;
    }
    return false;
}

void GameEngine::determineWinner() {
    // Build side pots from per-player contributions.
    // One pot is created per distinct contribution level (i.e. each all-in amount).
    // Eligible players for each pot = non-folded players who put in at least that level.
    struct SidePot { int amount; std::set<PlayerId> eligible; };
    std::vector<SidePot> pots;

    std::vector<int> levels;
    for (auto& [id, c] : m_state.totalContributed)
        if (c > 0) levels.push_back(c);
    std::sort(levels.begin(), levels.end());
    levels.erase(std::unique(levels.begin(), levels.end()), levels.end());

    int prevLevel = 0;
    for (int level : levels) {
        SidePot pot;
        for (auto& [id, c] : m_state.totalContributed)
            pot.amount += std::min(c, level) - std::min(c, prevLevel);
        for (auto& [id, c] : m_state.totalContributed)
            if (c >= level && m_state.foldedPlayers.count(id) == 0)
                pot.eligible.insert(id);
        if (!pot.eligible.empty())
            pots.push_back(pot);
        prevLevel = level;
    }

    // Award each pot to the best eligible hand
    for (auto& pot : pots) {
        // Uncontested: sole remaining player wins without needing community cards
        if (pot.eligible.size() == 1) {
            m_state.chipCounts[*pot.eligible.begin()] += pot.amount;
            continue;
        }

        // Contested: evaluate hands (requires all 5 community cards)
        PlayerId winner = -1;
        int bestScore = std::numeric_limits<int>::max();
        for (PlayerId id : pot.eligible) {
            if (m_state.holeCards.count(id) == 0) continue;
            std::vector<Card> seven;
            seven.push_back(m_state.holeCards[id].first);
            seven.push_back(m_state.holeCards[id].second);
            for (const Card& c : m_state.communityCards) seven.push_back(c);
            if (seven.size() == 7) {
                int score = HandEvaluator::evaluate(seven);
                if (score < bestScore) { bestScore = score; winner = id; }
            }
        }
        if (winner != -1)
            m_state.chipCounts[winner] += pot.amount;
    }
    m_state.pot = 0;
}

void GameEngine::tick() {
    // Worker thread calls tick() in a loop. Each call advances one street.
    // Showdown is a terminal state — start the next hand.
    if (m_state.street == Street::Showdown)
        startNewHand();
    else
        advanceStreet();
}

GameState GameEngine::getStateSnapshot() {
    std::lock_guard<std::mutex> lock(m_stateMutex);
    return m_state;
}

bool GameEngine::isGameOver() const {
    int playersWithChips = 0;
    for (auto& [id, chips] : m_state.chipCounts)
        if (chips > 0) ++playersWithChips;
    return playersWithChips <= 1;
}

} // namespace poker
```

---

**Q1.** The blinds are *forced* bets — they happen before any voluntary action. If blind players appeared in `needsToAct` like everyone else, the loop would ask them to act twice on pre-flop: once when posting (forced), and again when it's their turn to voluntarily raise or fold. Setting `currentBets` for them before the round starts means the loop sees their bets as already placed. The big blind still gets a chance to raise if no one else raised — this is handled because the big blind is *not* in `needsToAct` initially, but the loop checks "all bets equal" only after everyone in `needsToAct` has acted... actually, simpler: add the big blind back to `needsToAct` if the action returns to them without a raise. In practice, including the BB in `actionOrder` and letting the loop handle the "option" naturally is the correct design.

**Q2.** No. An all-in player has no chips to bet, so they cannot respond to a raise. They should not appear in `needsToAct`. In the implementation above, `buildActionOrder()` filters out `chipCounts[id] == 0`, so all-in players never enter the loop. Their bet stays at whatever they put in. Side pot calculation (for when the pot exceeds their all-in amount) is out of scope for v1.

</details>

---

### Task 3.7: Upgrade to the C++ `pheval` interface

After implementing Task 3.4 with the C API, you experienced its friction first-hand: a manual `rank * 4 + suit` encoding formula, a `- 2` offset to bridge your `Rank` enum to the library's 0-based range, and no type safety to catch mistakes. This is what working with raw C libraries feels like — it works, but it is fragile and error-prone.

The same library ships a proper C++ interface called `pheval` that wraps all of that away. Instead of computing integer encodings, you pass a string like `"Ac"` and the library handles the rest. Instead of a bare `int` return, you get a `phevaluator::Rank` object with methods like `.value()` and `.describeCategory()` (which returns `"Flush"`, `"Full House"`, etc. — useful for the UI later).

This task also introduces two important CMake patterns: `SOURCE_SUBDIR` (for libraries whose `CMakeLists.txt` is not at the repo root) and disabling a dependency's own test suite to avoid GoogleTest version conflicts. Both patterns come up regularly when integrating third-party libraries.

Now that you've felt the friction of the C API (raw integers, manual rank offset, no type safety), upgrade to the library's own C++ wrapper. Same library — just switching from the C bindings to the C++ ones.

- [ ] Update `cmake/Dependencies.cmake` — replace the manual `add_library` block with a `FetchContent_Declare(PokerHandEvaluator ...)` call. Use `GIT_TAG v0.5.3.1`, add `SOURCE_SUBDIR cpp` (the library's CMakeLists is not at the repo root), and set `BUILD_TESTS`, `BUILD_EXAMPLES`, `BUILD_PLO5`, and `BUILD_PLO6` to `OFF` before calling `FetchContent_MakeAvailable` to avoid a GoogleTest version conflict.

- [ ] Update `src/core/CMakeLists.txt` — change `poker_hand_evaluator` → `pheval`.

- [ ] Rewrite `HandEvaluator.cpp` to use the C++ interface: replace `toLibCard()` integer arithmetic with `phevaluator::Card(c.toShortString())`, and replace `evaluate_7cards()` with `phevaluator::EvaluateCards()`. Return `rank.value()` as the score.

- [ ] Clean build to verify the upgrade:
```bash
rm -rf build && cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build -j$(nproc)
```

- [ ] Commit:
```bash
git add cmake/Dependencies.cmake src/core/CMakeLists.txt src/core/HandEvaluator.cpp
git commit -m "refactor(core): upgrade PokerHandEvaluator to C++ interface (pheval)"
```

<details>
<summary>Concepts</summary>

> **Concept: `SOURCE_SUBDIR` in `FetchContent_Declare`**
>
> `FetchContent_Declare` by default looks for `CMakeLists.txt` in the root of the fetched repository. `PokerHandEvaluator`'s root has no `CMakeLists.txt` — the build system lives in the `cpp/` subdirectory. `SOURCE_SUBDIR cpp` tells CMake: "treat `cpp/` as the project root when configuring." Without it, CMake prints "does not appear to contain CMakeLists.txt" and the fetch fails.

> **Concept: `BUILD_TESTS OFF` — preventing a dependency conflict**
>
> `PokerHandEvaluator`'s own `CMakeLists.txt` optionally fetches GoogleTest to run its own tests. Our project also fetches GoogleTest. Two `FetchContent_Declare(googletest ...)` calls with different Git tags silently use whichever was declared first — but the resulting headers may be a version mismatch. Worse, the evaluator's own test targets pollute your CMake namespace.
>
> Setting `BUILD_TESTS OFF` before `FetchContent_MakeAvailable` tells the library's CMake not to add any test targets or fetch test dependencies. Always do this for any library that has its own test suite.

> **Concept: The reversed `operator<` on `phevaluator::Rank`**
>
> Inside the library, `Rank::operator<` is implemented as:
> ```cpp
> bool operator<(const Rank& other) const { return value_ > other.value_; }
> ```
>
> This is deliberately backwards. Since lower value = stronger hand, a "greater" rank in the library's ordering means a stronger hand. The reversal lets `std::max_element` find the *strongest* hand without a custom comparator:
>
> ```cpp
> // Find the player with the best hand using the library's Rank objects
> auto best = std::max_element(scores.begin(), scores.end());
> // best points to the strongest hand — no lambda needed
> ```
>
> Without the reversal, you'd need `std::min_element` or a comparator everywhere. This is a common C++ trick: define `operator<` in the direction that makes standard algorithms work naturally for your domain.

> **Concept: What changed between C and C++ API**
>
> | | C API | C++ API (`pheval`) |
> |---|---|---|
> | Card construction | `rank * 4 + suit` integer formula | `phevaluator::Card("Ac")` from string |
> | Rank offset | Manual `- 2` for our enum | Handled by string parsing |
> | Return type | `int` directly | `phevaluator::Rank` object |
> | Extra features | None | `rank.describeCategory()` → `"Flush"` etc. |
>
> The C++ interface eliminates the encoding arithmetic entirely. `c.toShortString()` delegates the conversion to `Card`, which knows its own representation. `HandEvaluator.cpp` becomes encoding-agnostic.

**Questions — think through these before checking answers:**
1. Why does the clean rebuild (`rm -rf build`) matter for this task specifically, rather than an incremental build?
2. `rank.describeCategory()` is a free bonus from the C++ API. Where in the game would you use it?

</details>

<details>
<summary>Answers</summary>

**Reference implementation — `cmake/Dependencies.cmake`:**

```cmake
# Disable the library's own tests/examples to avoid a googletest version conflict
set(BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_PLO5     OFF CACHE BOOL "" FORCE)
set(BUILD_PLO6     OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    PokerHandEvaluator
    GIT_REPOSITORY https://github.com/HenryRLee/PokerHandEvaluator.git
    GIT_TAG        v0.5.3.1
    SOURCE_SUBDIR  cpp
)
FetchContent_MakeAvailable(PokerHandEvaluator)
# Provides CMake target: pheval
```

**Reference implementation — `src/core/CMakeLists.txt`:** change `poker_hand_evaluator` → `pheval` in the `target_link_libraries` call.

**Reference implementation — `HandEvaluator.cpp` (C++ API):**

```cpp
#include "HandEvaluator.hpp"
#include <phevaluator/phevaluator.h>

namespace poker {

static phevaluator::Card toLibCard(const Card& c) {
    // phevaluator::Card accepts a two-char string: "Ac", "2d", etc.
    return phevaluator::Card(c.toShortString());
}

int HandEvaluator::evaluate(const std::vector<Card>& cards) {
    phevaluator::Rank rank = phevaluator::EvaluateCards(
        toLibCard(cards[0]), toLibCard(cards[1]),
        toLibCard(cards[2]), toLibCard(cards[3]),
        toLibCard(cards[4]), toLibCard(cards[5]),
        toLibCard(cards[6])
    );
    return rank.value();  // int 1–7462; lower = stronger
}

bool HandEvaluator::beats(const std::vector<Card>& a, const std::vector<Card>& b) {
    return evaluate(a) < evaluate(b);
}

} // namespace poker
```

---

**Q1.** CMake caches the result of `FetchContent_Declare` and `FetchContent_MakeAvailable` in `build/_deps/`. If a previous build downloaded the old `poker_hand_evaluator` target, an incremental build may still find and use the old cache entry — especially if you renamed the target from `poker_hand_evaluator` to `pheval`. A clean rebuild forces CMake to re-evaluate all `FetchContent` declarations from scratch, ensuring it fetches via `SOURCE_SUBDIR cpp` and provides the `pheval` target. For CMakeLists.txt changes that touch `FetchContent`, always clean-build when something doesn't link.

**Q2.** `rank.describeCategory()` returns a human-readable string like `"Flush"`, `"Full House"`, `"Pair"`, etc. The natural place to use it: the UI layer's `GameRenderer` to display the winning hand description at showdown (`"Alice wins with a Full House"`), and the AI layer's `PromptBuilder` to include the current best hand in the prompt (`"Your best hand so far: Flush"`). Neither use case exists yet, but the information is available for free from Phase 3 onward.

</details>

---

## Evaluation

Run `/phase-verify` to compile and get automated feedback on your implementation.

### Build checklist

| Step | Command | Success looks like |
|---|---|---|
| Build | `cmake --build build -j$(nproc)` | `[100%]` with no errors; `GameEngine.cpp`, `HandEvaluator.cpp` appear in build output |
| Verify core target | `cmake --build build --target poker_core` | No "undefined reference" to `GameEngine`, `HandEvaluator`, or `GameState` symbols |
| Smoke run | `./build/texas-holdem` | Exits 0 — `main.cpp` compiles and links against `poker_core` |
| pheval upgrade | After Task 3.6: `rm -rf build && cmake -S . -B build && cmake --build build -j$(nproc)` | Configures using `pheval` target; no `poker_hand_evaluator` references remain |

### Concept checklist

Answer these honestly about your own code:

- [ ] Does `GameState` use `std::map<PlayerId, Hand>` for hole cards and `std::map<PlayerId, int>` for chip counts and bets (not `std::vector` indexed by seat)?
- [ ] Does `GameState` use `std::set<PlayerId>` for `foldedPlayers` (not a `bool` array)?
- [ ] Does `PlayerView` contain `myHand` and `selfId` instead of the full `holeCards` map?
- [ ] Does `makePlayerView()` use `state.holeCards.at(forPlayer)` (throws on missing key) rather than `operator[]` (silently inserts a default)?
- [ ] Does `GameEngine::runBettingRound()` call `makePlayerView(m_state, actingId)` and pass the result to `player->getAction()`?
- [ ] Is the `IPlayer` stub's `getAction` declared as `virtual Action getAction(const PlayerView& view) = 0`?
- [ ] Is `m_stateMutex` declared `mutable` in `GameEngine`, and do I understand why `mutable` is needed on a `const` method?
- [ ] Does `getStateSnapshot()` lock `m_stateMutex` before reading `m_state` and return a copy (not a reference)?
- [ ] Did I write `runBettingRound()` so that a Raise re-opens action to all non-folded players (not just players who haven't acted yet)?
- [ ] Does `advanceStreet()` deal exactly 3 community cards on the Flop, 1 on the Turn, and 1 on the River?
- [ ] Is `poker_core` linked against `pheval` (Task 3.7) and does `HandEvaluator.cpp` include `<phevaluator/phevaluator.h>`?
- [ ] Did I write the state machine transition comment at the top of `GameEngine.hpp` (Task 3.3) before implementing the logic?
- [ ] Are `GameEngine.cpp` and `HandEvaluator.cpp` listed in `src/core/CMakeLists.txt`?
- [ ] Does `Card::toShortString()` return `"As"` for Ace of Spades and `"2h"` for Two of Hearts?

### Common mistakes

| Mistake | Symptom | Fix |
|---|---|---|
| Forgetting `inline` on `makePlayerView()` | Linker error: "multiple definition of `poker::makePlayerView`" when `PlayerView.hpp` is included in more than one `.cpp` | Add `inline` before the function definition in the header |
| Using `state.holeCards[forPlayer]` instead of `.at(forPlayer)` in `makePlayerView` | `operator[]` silently inserts a default-constructed `Hand` when the key is missing; `makePlayerView` returns garbage cards with no compiler warning | Use `.at(forPlayer)` — it throws `std::out_of_range` if called before cards are dealt, making the bug visible |
| Passing `m_state` directly to `player->getAction()` instead of a `PlayerView` | Compile error: `IPlayer::getAction` expects `const PlayerView&`, not `const GameState&` | Call `makePlayerView(m_state, actingId)` first and pass the result |
| Forgetting `mutable` on `m_stateMutex` | Compiler error: "passing `const std::mutex` as `this` discards qualifiers" inside `getStateSnapshot()` | Declare `mutable std::mutex m_stateMutex;` |
| `getStateSnapshot()` returns `const GameState&` instead of `GameState` | Data race: renderer reads stale or partially-written data | Return `GameState` by value — the copy is made under the lock |
| Rank enum offset not adjusted in C API path | `HandEvaluator` returns nonsensical or identical scores for all hands | In `toLibCard()`, subtract 2: `static_cast<int>(c.getRank()) - 2` |
| Betting round never ends | Game hangs after pre-flop; `tick()` blocks forever | The loop exit requires ALL active players to have acted AND all `currentBets` equal; check both conditions |
| `GameEngine.cpp` or `HandEvaluator.cpp` not in CMakeLists | "Undefined reference to `poker::GameEngine::GameEngine`" at link time | Add both `.cpp` filenames to `add_library(poker_core STATIC ...)` in `src/core/CMakeLists.txt` |
| `BUILD_TESTS OFF` omitted before `FetchContent_MakeAvailable` | CMake version conflict on GoogleTest; duplicate target errors | Add the four `set(BUILD_* OFF ...)` lines before `FetchContent_MakeAvailable(PokerHandEvaluator)` |
| Missing `SOURCE_SUBDIR cpp` in `FetchContent_Declare` | "does not appear to contain CMakeLists.txt" error during configure | Add `SOURCE_SUBDIR cpp` to the `FetchContent_Declare(PokerHandEvaluator ...)` call |

### Self-score

- **Solid**: built first try, `GameEngine` state machine logic correct on first read-through, mutex and `mutable` understood, answered all checklist questions without looking things up.
- **Learning**: built after fixing 1–2 errors (missing `mutable`, wrong return type on snapshot, CMakeLists omission, rank offset), betting round needed a second pass for the re-open-on-raise rule.
- **Needs review**: multiple linker errors, threading concepts unclear, betting loop logic incorrect or infinite — re-read the Concepts sections for Tasks 3.2 (state machine), 3.3 (mutex/mutable), and 3.5 (re-open-on-raise) before moving to Phase 4.
