# Texas Hold'em AI Pro — Design Spec

**Date:** 2026-03-22
**Status:** Approved

---

## Overview

A Texas Hold'em poker game written in C++ with SFML for graphics and LLM-powered AI players via any OpenAI-compatible local LLM server (Ollama, llama.cpp, etc.). The primary goals are learning modern C++ development practices and building extensible LLM AI players, with a polished playable game as the vehicle.

---

## Goals & Priorities

1. **Learn modern C++** — CMake, interfaces, RAII, smart pointers, STL, clean architecture
2. **LLM AI players** — AI players backed by any OpenAI-compatible LLM server (Ollama, llama.cpp), extensible to cloud APIs (Anthropic Claude, OpenAI)
3. **Playable game** — Human vs AI Texas Hold'em with SFML UI
4. **Future extensibility** — web UI, event-driven architecture, async AI calls, multiple LLM backends

---

## Architecture: Layered (Approach B)

Three clearly separated layers communicating through well-defined interfaces:

```
┌─────────────────────────────────────┐
│         UI Layer (SFML)             │  Renders state, captures human input
├─────────────────────────────────────┤
│       Players Layer                 │  IPlayer interface, Human + AI players
├─────────────────────────────────────┤
│         Core Layer                  │  Pure game logic, no UI or network deps
└─────────────────────────────────────┘
         ↕ (used by players layer)
┌─────────────────────────────────────┐
│         AI Layer                    │  ILLMClient interface, OpenAI-compatible backends
└─────────────────────────────────────┘
```

**Key principle:** `core/` has zero dependencies on `ui/`, `players/`, or `ai/`. `GameEngine` only knows `IPlayer`. `AIPlayer` only knows `ILLMClient`. Players layer does NOT depend on UI layer — `HumanPlayer` communicates with the UI via a shared `std::promise<Action>` / callback mechanism.

---

## Project Structure

```
texas-holdem-aipro/
├── CMakeLists.txt
├── cmake/
│   └── Dependencies.cmake          # FetchContent: SFML, nlohmann_json, googletest, yaml-cpp, cpp-httplib
├── config/
│   ├── game.yaml                   # AI model, hand evaluator, advanced settings
│   └── personalities/
│       ├── default.md              # Default AI prompt/personality
│       ├── aggressive.md
│       └── cautious.md
├── src/
│   ├── core/
│   │   ├── Types.hpp               # PlayerId, Action, Street, shared value types
│   │   ├── Card.hpp/cpp            # Suit, rank, string representation
│   │   ├── Deck.hpp/cpp            # 52-card deck, shuffle, deal
│   │   ├── Hand.hpp/cpp            # A player's two hole cards
│   │   ├── HandEvaluator.hpp/cpp   # Wraps hand evaluation library
│   │   ├── GameState.hpp/cpp       # Full game state (see Component Designs)
│   │   └── GameEngine.hpp/cpp      # State machine: pre-flop→flop→turn→river→showdown
│   ├── players/
│   │   ├── IPlayer.hpp             # Pure interface: getId, dealHoleCards, getAction (getName added Phase 6)
│   │   ├── HumanPlayer.hpp/cpp     # Blocks on std::future<Action> fulfilled by InputHandler
│   │   └── AIPlayer.hpp/cpp        # Builds prompt, calls ILLMClient, parses response
│   ├── ai/
│   │   ├── ILLMClient.hpp                  # Interface + LLMConfig struct
│   │   ├── OpenAICompatibleClient.hpp/cpp  # OpenAI /v1/chat/completions via cpp-httplib
│   │   └── PromptBuilder.hpp/cpp           # Serializes GameState → user message string
│   ├── ui/
│   │   ├── GameRenderer.hpp/cpp    # Draws table, cards, chips, player info
│   │   ├── SetupScreen.hpp/cpp     # Startup UI: player count, stacks, blinds
│   │   └── InputHandler.hpp/cpp    # Keyboard/mouse → fulfills HumanPlayer's promise
│   └── main.cpp                    # Wires everything together, reads config
├── assets/
│   ├── cards/                      # Card sprites
│   └── fonts/
├── tests/
│   ├── core/                       # Unit tests: HandEvaluator, GameEngine, GameState
│   ├── players/                    # AIPlayer with MockLLMClient
│   └── ai/                         # PromptBuilder output correctness
├── docs/
│   └── superpowers/specs/
└── third_party/                    # All via CMake FetchContent (no manual downloads)
```

---

## Component Designs

### `core/Types.hpp` — Shared Value Types

All fundamental types live here to avoid circular includes:

```cpp
using PlayerId = int;   // opaque player identifier — never used in arithmetic, only as a map key
// SeatIndex (0..n-1) is an internal GameEngine concern used for rotation arithmetic;
// it never appears in GameState or any public interface

enum class Street { PreFlop, Flop, Turn, River, Showdown };

struct Action {
    enum class Type { Fold, Call, Raise } type;
    int amount = 0;  // chips; only meaningful for Raise. Must be >= minRaise, clamped by engine.
                     // Call does NOT carry an amount — GameEngine computes call cost from GameState::currentBets.
};
```

**Money representation:** All chip values are `int` (non-negative). Valid range: 0 to total chips in the game. The `GameEngine` validates `Action::amount` on raises — clamping to `[minRaise, playerStack]` before applying. Negative values are rejected.

---

### `core/Card.hpp`

Value type. `Suit` enum (Hearts/Diamonds/Clubs/Spades), `Rank` enum (Two–Ace). Equality comparable, totally ordered, stringifiable. No heap allocation.

---

### `core/Deck.hpp`

Owns 52 `Card` values. `shuffle()` uses `std::mt19937` seeded from `std::random_device`. `deal()` returns the top card and removes it. `reset()` restores all 52 cards.

---

### `core/Hand.hpp`

Represents a player's two private hole cards:
```cpp
struct Hand {
    Card first;
    Card second;
};
```
Simple value type. Used in `GameState` and `IPlayer::dealHoleCards`.

---

### `core/GameState.hpp`

Central data structure passed (as `const` ref) to `IPlayer::getAction` and `PromptBuilder`. Contains:

```cpp
struct GameState {
    // Cards
    std::vector<Card>          communityCards;   // 0–5 cards depending on street
    std::map<PlayerId, Hand>   holeCards;        // private — AI only reads its own

    // Positions
    PlayerId                   dealerButton;
    PlayerId                   smallBlindSeat;
    PlayerId                   bigBlindSeat;
    std::vector<PlayerId>      actionOrder;      // clockwise from left of dealer for this street

    // Betting
    std::map<PlayerId, int>    chipCounts;       // current stack per player
    std::map<PlayerId, int>    currentBets;      // amount bet this street per player
    int                        pot;
    int                        minRaise;         // minimum legal raise amount this street

    // State
    Street                     street       = Street::PreFlop;
    PlayerId                   activePlayer = 0;
    std::set<PlayerId>         foldedPlayers;

    // Side pot tracking: total chips committed per player across all streets this hand.
    // Used by determineWinner() to calculate per-player pot eligibility.
    std::map<PlayerId, int>    totalContributed;

    // Renderer signal: true while the engine is blocked waiting for activePlayer's action.
    // The renderer uses this to show a "thinking..." or "your turn" overlay.
    // Set true (under lock) before calling getAction(); set false (under lock) after applying it.
    bool                       waitingForAction = false;
};
```

**Visibility rule:** `GameEngine` never passes `GameState` to players directly. Before calling `IPlayer::getAction()`, the engine calls `makePlayerView(state, playerId)` to produce a `PlayerView` — a filtered snapshot containing the calling player's own hole cards but not the `holeCards` map. This enforces information hiding at the type level: opponent cards are structurally absent, not just hidden by convention.

---

### `core/PlayerView.hpp`

A filtered snapshot of `GameState` passed to `IPlayer::getAction()` and `PromptBuilder::build()`. Contains all public game information (community cards, chip counts, bets, positions, street) but replaces the full `holeCards` map with a single `myHand` field for the calling player only.

```cpp
struct PlayerView {
    std::vector<Card>        communityCards;
    PlayerId                 dealerButton;
    PlayerId                 smallBlindSeat;
    PlayerId                 bigBlindSeat;
    std::vector<PlayerId>    actionOrder;
    std::map<PlayerId, int>  chipCounts;
    std::map<PlayerId, int>  currentBets;
    int                      pot;
    int                      minRaise;
    Street                   street;
    PlayerId                 activePlayer;
    std::set<PlayerId>       foldedPlayers;
    PlayerId                 myId;     // which player this view is for
    Hand                     myHand;   // only this player's hole cards
};

// Precondition: state.holeCards.count(forPlayer) > 0
inline PlayerView makePlayerView(const GameState& state, PlayerId forPlayer);
```

`makePlayerView` is an `inline` free function defined in `PlayerView.hpp` — it is header-only, no `.cpp` needed. `PromptBuilder` receives a `PlayerView` so it cannot accidentally include opponent hole cards in an AI prompt even through a coding error.

---

### `core/HandEvaluator.hpp`

Thin wrapper around the [HenryRLee/PokerHandEvaluator](https://github.com/HenryRLee/PokerHandEvaluator) C library. Evaluates best 5-card hand from up to 7 cards. Returns a numeric rank (lower = stronger, library convention).

**CMake integration note:** This library requires custom `CMakeLists.txt` wrapping — it is not `FetchContent_MakeAvailable` out of the box. Phase 1 includes writing this wrapper as a CMake learning exercise with step-by-step guidance.

**Learning exercise:** A template for self-implementing a hand evaluator (using bit manipulation and combinatorics) is provided separately as a guided C++ exercise, independent of the main build.

---

### `core/GameEngine.hpp`

State machine. Drives the full game loop. Holds `vector<unique_ptr<IPlayer>>` and the authoritative `GameState`.

**Internal design — identity vs position:**
`GameEngine` uses a private `SeatIndex` type (0..n-1) for all seat rotation arithmetic. This is kept entirely internal — it never appears in `GameState` or any public interface. `PlayerId` is opaque; arithmetic on it is meaningless.

```cpp
// Private to GameEngine — never exposed publicly
using SeatIndex = int;
std::vector<PlayerId>         m_seats;      // seat index → PlayerId
std::map<PlayerId, SeatIndex> m_seatOf;     // PlayerId → seat index
SeatIndex                     m_dealerSeat; // rotation state; initialised to n-1 so first rotation lands on seat 0
```

**Construction and startup:**
The constructor only initialises data structures — it does not start a hand. `m_handStarted` (private `bool`, defaults to `false`) tracks whether the first hand has begun. The first call to `tick()` triggers `startNewHand()` via the existing Showdown branch — no special-casing required. This keeps the object observable: callers can inspect initial chip counts before any game logic runs.

**Betting round algorithm:**
1. Determine action order for the street (pre-flop: UTG first; post-flop: first active player left of dealer)
2. For each active (non-folded) player:
   - **Lock** → set `activePlayer`, set `waitingForAction = true` → **unlock** (renderer sees "waiting" state)
   - Call `IPlayer::getAction(view)` — **no lock held** during this call (may block for seconds on human input or AI HTTP)
   - **Lock** → set `waitingForAction = false`, apply action to `GameState` → **unlock** (renderer sees updated state)
3. A `Raise` reopens action to all players who have not yet folded (they get another turn)
4. Street ends when all active players have acted AND all `currentBets` are equal (or player is all-in)
5. Advance to next street; reset `currentBets`, recalculate `actionOrder`

**Why the lock is released around `getAction()`:** If the engine held the mutex for the entire betting round, `getStateSnapshot()` would block until the full round completed — the renderer could never see individual actions. Releasing the lock before each `getAction()` call lets the renderer read a fresh snapshot after every state change, enabling smooth per-action animation. `waitingForAction` in `GameState` tells the renderer whether to show a static "player acted" frame or a live waiting animation.

**All-in / side pots:** Full side pot calculation is implemented using per-player `totalContributed` tracking. `determineWinner()` builds one `SidePot` per distinct contribution level, determines eligible players (those who contributed at least that level and did not fold), and awards each pot independently to the best eligible hand. This handles all-in players correctly.

**Showdown:** `GameEngine` uses `HandEvaluator` to compare remaining players' best 5-card hands (from their hole cards + community cards) and distributes the pot to the winner(s).

---

### `players/IPlayer.hpp`

Pure abstract interface. All player types implement this:

```cpp
class IPlayer {
public:
    virtual ~IPlayer() = default;
    virtual PlayerId    getId()                              const = 0;
    virtual void        dealHoleCards(const Hand& cards)          = 0;  // engine delivers private cards
    virtual Action      getAction(const PlayerView& view)         = 0;  // filtered: only own hole cards visible
    // virtual std::string getName() const = 0;  // added Phase 6 for UI display
};
```

---

### `players/HumanPlayer.hpp`

Implements `IPlayer`. On `getAction()`, sets a `std::promise<Action>` and blocks on the corresponding `std::future<Action>`. The `InputHandler` (UI layer) holds a reference to the promise and fulfills it when the user clicks Fold/Call/Raise.

This keeps `HumanPlayer` independent of SFML — it only knows about `std::promise`, not about windows or events.

---

### `players/AIPlayer.hpp`

Implements `IPlayer`. Holds an `ILLMClient&` and a personality file path.

On `getAction()`:
1. Calls `PromptBuilder::build(state, ownId, personalityText)`
2. Calls `ILLMClient::sendPrompt(prompt)` — synchronous, game waits
3. Parses response string into an `Action`
4. On parse failure: retries once, then defaults to `Action::Call`
5. `GameRenderer` shows a "thinking..." overlay while this call blocks

---

### `ai/ILLMClient.hpp`

```cpp
class ILLMClient {
public:
    virtual ~ILLMClient() = default;
    virtual std::string sendPrompt(const std::string& prompt) = 0;
};
```

---

### `ai/OllamaClient.hpp`

Implements `ILLMClient`. Uses `cpp-httplib` (single-header, FetchContent-friendly) to POST to `http://localhost:11434/api/generate`. Model name read from `config/game.yaml`. Response parsed with `nlohmann/json`.

Error handling: on HTTP failure, returns empty string → `AIPlayer` fallback triggers.

---

### `ai/PromptBuilder.hpp`

Takes `GameState`, `PlayerId ownId`, and personality markdown text. Produces a prompt string containing:
- Personality/play style instructions (from `.md` file)
- Own hole cards
- Community cards
- Current pot and bet amounts
- Each player's chip count
- Action history for this street
- Legal actions available (Fold / Call $X / Raise $Y–$Z)
- Instruction to respond with exactly: `FOLD`, `CALL`, or `RAISE <amount>`

---

### `ai/ClaudeClient.hpp`

Stubbed. Implements `ILLMClient`. Targets Anthropic REST API. Activated by setting `llm.backend: claude` in `config/game.yaml`.

---

### `ui/SetupScreen.hpp`

First screen on launch. SFML form collecting:
- Number of AI players (1–8)
- Starting chip stack (all players start equal)
- Small blind / big blind amounts

On submit, returns a `GameConfig` struct to `main.cpp`.

---

### `ui/GameRenderer.hpp`

Calls `GameEngine::getStateSnapshot()` on every frame to get a fresh copy of `GameState`. The snapshot must not be cached across frames — the engine publishes a new state after each individual player action. Renders:
- Table felt and layout
- Community cards (face up)
- Each player's position, name, chip count
- Current bets and pot
- Human player's hole cards (face up); AI hole cards (face down)
- Active player indicator (always `state.activePlayer`)
- **Waiting animation:** when `state.waitingForAction == true`:
  - Human player (`activePlayer == humanId`): show Fold/Call/Raise action buttons
  - AI player: show "Thinking..." overlay on that player's seat
- Win/fold overlays at showdown

Because `GameEngine` releases its mutex before calling `getAction()` and re-acquires it after applying the result, the renderer sees two distinct published states per action: one with `waitingForAction = true` (player deciding) and one with `waitingForAction = false` and the action already applied (chips moved, player folded, etc.). This gives the renderer enough information to animate every step cleanly.

---

### `ui/InputHandler.hpp`

Maps SFML mouse/keyboard events to game actions. When it is the human player's turn, shows Fold/Call/Raise buttons and a raise amount input. On button click, fulfills the `std::promise<Action>` held by `HumanPlayer`.

---

## Data Flow

```
main.cpp
  └── reads config/game.yaml (yaml-cpp)
  └── shows SetupScreen → collects GameConfig {numAI, startingStack, smallBlind, bigBlind}
  └── constructs vector<unique_ptr<IPlayer>> (1 HumanPlayer + N AIPlayers)
  └── constructs GameEngine(players, config)
  └── worker thread: GameEngine::tick() loop
        └── lock → set activePlayer, waitingForAction=true → unlock
        └── makePlayerView(state, activePlayerId) → PlayerView (filtered: own cards only)
        └── IPlayer::getAction(view)  ← no lock held during this call
              HumanPlayer → blocks on std::future<Action>
                ← InputHandler (main thread) fulfills promise on button click
              AIPlayer → PromptBuilder::build(view, personality) → OllamaClient (sync HTTP) → parse → Action
        └── lock → waitingForAction=false, apply Action to GameState → unlock
        └── advances street or hand as needed

  └── main thread: SFML event loop
        └── GameRenderer::render(engine.getStateSnapshot())  ← every frame
              reads waitingForAction → shows "Thinking..." / action buttons / idle
        └── InputHandler::handleEvents() → fulfills HumanPlayer promise on click
```

**Threading model:** `GameEngine` runs on a dedicated worker thread. The main thread runs the SFML event pump and calls `GameRenderer::render()` every frame via `GameEngine::getStateSnapshot()`.

The mutex (`m_stateMutex`) is held only for short critical sections — never across a `getAction()` call, which can block for seconds. The pattern inside each action step:

```
worker thread                          main thread
─────────────────────────────────      ──────────────────────────
lock → set activePlayer,               getStateSnapshot() → copy
        waitingForAction=true           (may briefly wait for lock)
unlock                            ←──  reads "waiting" state, shows animation

getAction() ← blocks here             getStateSnapshot() → copy
  HumanPlayer: waits on future         reads "waiting" state again (free, no lock needed)
  AIPlayer: synchronous HTTP

lock → waitingForAction=false,
        apply action to GameState
unlock                            ←──  getStateSnapshot() → copy
                                        reads updated state, animates result
```

`InputHandler` runs on the main thread and fulfills `HumanPlayer`'s `std::promise<Action>` on button click, unblocking the worker thread. `waitingForAction` in `GameState` tells the renderer which overlay to show — action buttons for the human player, "Thinking..." for AI.

---

## Configuration

**`config/game.yaml`** — Advanced/technical settings:
```yaml
llm:
  backend: ollama           # ollama | claude (future)
  model: llama3.2
  endpoint: http://localhost:11434

hand_evaluator:
  library: poker-hand-evaluator   # library | custom (self-implementation exercise)

logging:
  level: info               # debug | info | warn | error
```

**`config/personalities/*.md`** — Plain markdown files injected into AI prompts. Editable without recompiling. Each AI player is assigned a personality at startup.

**Startup UI (SetupScreen)** — Collects: player count, starting stacks, blind amounts. No config file for these.

---

## Error Handling

Minimal, designed for future extension:
- LLM HTTP failure → `OllamaClient` returns empty string → `AIPlayer` retries once → falls back to `Action::Call`
- Malformed LLM response → same retry/fallback path
- `ILLMClient` interface makes it easy to add retry logic, timeouts, or circuit breakers later

---

## Testing Strategy

**Framework:** GoogleTest (GTest + GMock, via CMake FetchContent)

| Test area | What's tested |
|---|---|
| `tests/core/` | `HandEvaluator` with known hand fixtures, `GameEngine` state machine transitions, `GameState` mutation correctness, betting round logic |
| `tests/players/` | `AIPlayer` with `MockLLMClient` injected via `ILLMClient`, response parsing, fallback on failure |
| `tests/ai/` | `PromptBuilder` output format, personality injection, visibility (no opponent hole cards in prompt) |

No mocking of deck or game engine in integration tests — test against real logic.

---

## Build System

CMake 3.21+ with `FetchContent` for all dependencies. No manual downloads.

**Dependencies:**
| Library | Purpose | Integration |
|---|---|---|
| SFML 2.6 | Graphics, window, input | FetchContent (requires system OpenGL, freetype: `sudo apt install libsfml-dev` or equivalent) |
| nlohmann/json | JSON parsing (Ollama responses) | FetchContent (header-only) |
| yaml-cpp | Parsing `config/game.yaml` | FetchContent |
| cpp-httplib | HTTP client for OllamaClient | FetchContent (single-header) |
| GoogleTest | Unit testing (GTest + GMock) | FetchContent |
| PokerHandEvaluator | Hand strength evaluation | FetchContent with manual CMake wrapper (Phase 1); upgraded to C++ interface (`pheval`) in Phase 3 |

**Note on SFML system deps:** SFML source is fetched by CMake, but it links against system libraries (OpenGL, freetype, etc.) that must be installed separately. Linux: `sudo apt install libsfml-dev` installs all prerequisites.

---

## Phased Delivery

| Phase | Deliverable |
|---|---|
| 1 | CMake scaffold, project structure, FetchContent for all deps including PokerHandEvaluator wrapper |
| 2 | Core layer: Types, Card, Deck, Hand |
| 3 | Core layer: GameState, GameEngine, PlayerView, HandEvaluator (C++ pheval interface) |
| 4 | Core layer tests with GoogleTest — GameEngine state machine, betting round logic |
| 5 | AI layer: ILLMClient, OllamaClient (cpp-httplib), PromptBuilder + personality .md files |
| 6 | Players layer: IPlayer, HumanPlayer (promise/future), AIPlayer (uses ILLMClient) |
| 7 | Players layer tests: AIPlayer with MockLLMClient |
| 8 | UI layer: SetupScreen, GameRenderer, InputHandler |
| 9 | Wire everything in main.cpp — full playable game |
| 10 | Polish: additional personality files, prompt tuning, UI refinements |

---

## Future Extensions (Out of Scope Now)

- Async AI calls (`std::async`, thread pool)
- Web UI frontend (C++ HTTP backend, swap UI layer)
- Additional LLM backends (Claude, OpenAI) — `ClaudeClient` stub already in place
- Event-driven architecture (Approach C)
- Spectator/AI-vs-AI mode
- Hand replay via event stream recording
- Self-implemented hand evaluator (guided C++ learning exercise)
- Logging library integration (e.g. spdlog)
