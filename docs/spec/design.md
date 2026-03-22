# Texas Hold'em AI Pro — Design Spec

**Date:** 2026-03-22
**Status:** Approved

---

## Overview

A Texas Hold'em poker game written in C++ with SFML for graphics and LLM-powered AI players via a local Ollama instance. The primary goals are learning modern C++ development practices and building extensible LLM AI players, with a polished playable game as the vehicle.

---

## Goals & Priorities

1. **Learn modern C++** — CMake, interfaces, RAII, smart pointers, STL, clean architecture
2. **LLM AI players** — AI players backed by a local language model (Ollama), extensible to cloud APIs (Anthropic Claude, OpenAI)
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
│         AI Layer                    │  ILLMClient interface, Ollama + future backends
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
│   │   ├── IPlayer.hpp             # Pure interface: getId, getName, dealHoleCards, getAction
│   │   ├── HumanPlayer.hpp/cpp     # Blocks on std::future<Action> fulfilled by InputHandler
│   │   └── AIPlayer.hpp/cpp        # Builds prompt, calls ILLMClient, parses response
│   ├── ai/
│   │   ├── ILLMClient.hpp          # Interface: sendPrompt(string) → string
│   │   ├── OllamaClient.hpp/cpp    # HTTP POST to local Ollama REST API via cpp-httplib
│   │   ├── ClaudeClient.hpp/cpp    # Future: Anthropic cloud API
│   │   └── PromptBuilder.hpp/cpp   # Serializes GameState + personality → prompt string
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
using PlayerId = int;   // 0-based seat index

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
    Street                     street;
    PlayerId                   activePlayer;
    std::set<PlayerId>         foldedPlayers;
};
```

**Visibility rule:** `AIPlayer` and `HumanPlayer` receive the full `GameState` but MUST only read `holeCards[ownId]`. It is the `PromptBuilder`'s responsibility to exclude opponent hole cards from the prompt. `GameEngine` owns the authoritative `GameState`. This is a trust-based contract enforced by convention, not by the type system. A filtered `PlayerView` struct (containing only the calling player's hole cards) is a straightforward future hardening step.

---

### `core/HandEvaluator.hpp`

Thin wrapper around the [HenryRLee/PokerHandEvaluator](https://github.com/HenryRLee/PokerHandEvaluator) C library. Evaluates best 5-card hand from up to 7 cards. Returns a numeric rank (lower = stronger, library convention).

**CMake integration note:** This library requires custom `CMakeLists.txt` wrapping — it is not `FetchContent_MakeAvailable` out of the box. Phase 1 includes writing this wrapper as a CMake learning exercise with step-by-step guidance.

**Learning exercise:** A template for self-implementing a hand evaluator (using bit manipulation and combinatorics) is provided separately as a guided C++ exercise, independent of the main build.

---

### `core/GameEngine.hpp`

State machine. Drives the full game loop. Holds `vector<unique_ptr<IPlayer>>` and the authoritative `GameState`.

**Betting round algorithm:**
1. Determine action order for the street (pre-flop: UTG first; post-flop: first active player left of dealer)
2. Each active (non-folded) player is asked for an `Action` via `IPlayer::getAction(state)`
3. A `Raise` reopens action to all players who have not yet folded (they get another turn)
4. Street ends when all active players have acted AND all `currentBets` are equal (or player is all-in)
5. Advance to next street; reset `currentBets`, recalculate `actionOrder`

**All-in / side pots:** v1 scopes out side pot calculation. If a player goes all-in for less than the current bet, the engine awards them only the portion of the pot they are eligible for. Full side-pot algorithm is a future extension.

**Showdown:** `GameEngine` uses `HandEvaluator` to compare remaining players' best 5-card hands (from their hole cards + community cards) and distributes the pot to the winner(s).

---

### `players/IPlayer.hpp`

Pure abstract interface. All player types implement this:

```cpp
class IPlayer {
public:
    virtual ~IPlayer() = default;
    virtual PlayerId    getId()   const = 0;
    virtual std::string getName() const = 0;
    virtual void        dealHoleCards(const Hand& cards) = 0;  // engine delivers private cards
    virtual Action      getAction(const GameState& state) = 0;
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

Reads `const GameState&` each frame. Renders:
- Table felt and layout
- Community cards (face up)
- Each player's position, name, chip count
- Current bets and pot
- Human player's hole cards (face up); AI hole cards (face down)
- Active player indicator
- "Thinking..." overlay when `AIPlayer::getAction` is running
- Win/fold overlays at showdown

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
  └── game loop:
        GameEngine::tick()
          └── requests Action from active IPlayer::getAction(state)
                HumanPlayer → blocks on std::future<Action>
                  ← InputHandler fulfills promise on button click
                AIPlayer → PromptBuilder → OllamaClient (sync HTTP) → parse → Action
          └── validates and applies Action to GameState
          └── advances street or hand as needed
          └── GameRenderer::render(gameState)  ← called every frame from main loop
```

**Threading model:** `GameEngine::tick()` blocks the calling thread (on `std::future` for human input, on synchronous HTTP for AI). To keep the SFML window responsive, `GameEngine` runs on a dedicated worker thread. The main thread runs the SFML event pump and calls `GameRenderer::render()` every frame, reading a snapshot of `GameState` protected by a mutex. `InputHandler` runs on the main thread and fulfills `HumanPlayer`'s promise via the shared promise reference. This design prevents the window from freezing during AI "thinking" time and allows the "Thinking..." overlay to render correctly. Thread safety is scoped to the `GameState` snapshot — `GameEngine` writes state on its thread, `GameRenderer` reads a copy on the main thread.

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
| 2 | Core layer: Types, Card, Deck, Hand, GameState, GameEngine (no UI, no AI) |
| 3 | Core layer tests with GoogleTest — GameEngine state machine, betting round logic |
| 4 | AI layer: ILLMClient, OllamaClient (cpp-httplib), PromptBuilder + personality .md files |
| 5 | Players layer: IPlayer, HumanPlayer (promise/future), AIPlayer (uses ILLMClient) |
| 6 | Players layer tests: AIPlayer with MockLLMClient |
| 7 | UI layer: SetupScreen, GameRenderer, InputHandler |
| 8 | Wire everything in main.cpp — full playable game |
| 9 | Polish: additional personality files, prompt tuning, UI refinements |

---

## Future Extensions (Out of Scope Now)

- Async AI calls (`std::async`, thread pool)
- Web UI frontend (C++ HTTP backend, swap UI layer)
- Additional LLM backends (Claude, OpenAI) — `ClaudeClient` stub already in place
- Event-driven architecture (Approach C)
- Spectator/AI-vs-AI mode
- Hand replay via event stream recording
- Full side-pot calculation
- Self-implemented hand evaluator (guided C++ learning exercise)
- Logging library integration (e.g. spdlog)
