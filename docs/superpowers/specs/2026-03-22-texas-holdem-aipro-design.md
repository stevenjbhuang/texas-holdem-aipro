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

**Key principle:** `core/` has zero dependencies on `ui/`, `players/`, or `ai/`. `GameEngine` only knows `IPlayer`. `AIPlayer` only knows `ILLMClient`.

---

## Project Structure

```
texas-holdem-aipro/
├── CMakeLists.txt
├── cmake/
│   └── Dependencies.cmake          # FetchContent: SFML, nlohmann_json, Catch2, yaml-cpp
├── config/
│   ├── game.yaml                   # AI model, hand evaluator, advanced settings
│   └── personalities/
│       ├── default.md              # Default AI prompt/personality
│       ├── aggressive.md
│       └── cautious.md
├── src/
│   ├── core/
│   │   ├── Card.hpp/cpp            # Suit, rank, string representation
│   │   ├── Deck.hpp/cpp            # 52-card deck, shuffle, deal
│   │   ├── Hand.hpp/cpp            # A player's hole cards
│   │   ├── HandEvaluator.hpp/cpp   # Wraps hand evaluation library
│   │   ├── GameState.hpp/cpp       # Community cards, pot, bets, chip counts, street
│   │   └── GameEngine.hpp/cpp      # State machine: pre-flop→flop→turn→river→showdown
│   ├── players/
│   │   ├── IPlayer.hpp             # Pure interface: getAction(GameState) → Action
│   │   ├── HumanPlayer.hpp/cpp     # Delegates to UI InputHandler
│   │   └── AIPlayer.hpp/cpp        # Builds prompt, calls ILLMClient, parses response
│   ├── ai/
│   │   ├── ILLMClient.hpp          # Interface: sendPrompt(string) → string
│   │   ├── OllamaClient.hpp/cpp    # HTTP POST to local Ollama REST API
│   │   ├── ClaudeClient.hpp/cpp    # Future: Anthropic cloud API
│   │   └── PromptBuilder.hpp/cpp   # Serializes GameState → prompt string
│   ├── ui/
│   │   ├── GameRenderer.hpp/cpp    # Draws table, cards, chips, player info
│   │   ├── SetupScreen.hpp/cpp     # Startup UI: player count, stacks, blinds
│   │   └── InputHandler.hpp/cpp    # Keyboard/mouse → Action events
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

### Core Layer

**`Card`** — Value type. Suit enum (Hearts/Diamonds/Clubs/Spades), Rank enum (2–Ace). Equality comparable, stringifiable.

**`Deck`** — Owns 52 `Card`s. `shuffle()` uses `std::mt19937` with random seed. `deal()` returns top card.

**`GameState`** — Central data structure passed everywhere. Contains:
- `std::vector<Card> communityCards`
- `std::map<PlayerId, int> chipCounts`
- `std::map<PlayerId, int> currentBets`
- `int pot`
- `Street street` (PreFlop / Flop / Turn / River / Showdown)
- `PlayerId activePlayer`

**`GameEngine`** — State machine. Drives the game loop, requests actions from `IPlayer`, advances street, determines winner via `HandEvaluator`, distributes pot.

**`HandEvaluator`** — Thin wrapper around [PokerHandEvaluator](https://github.com/HenryRLee/PokerHandEvaluator) library. Evaluates best 5-card hand from 7 cards. A learning template with instructions for self-implementation is provided separately.

### Players Layer

**`IPlayer`** — Pure abstract interface:
```cpp
struct Action { enum Type { Fold, Call, Raise } type; int amount; };
virtual Action getAction(const GameState& state) = 0;
virtual std::string getName() const = 0;
```

**`HumanPlayer`** — Waits for `InputHandler` to produce an `Action` (button click or keyboard).

**`AIPlayer`** — Uses `PromptBuilder` to serialize `GameState` into a prompt, sends via `ILLMClient`, parses the response into an `Action`. Sync call (game waits, UI shows "thinking...").

### AI Layer

**`ILLMClient`** — Single method interface: `virtual std::string sendPrompt(const std::string& prompt) = 0;`

**`OllamaClient`** — HTTP POST to `localhost:11434/api/generate`. Uses `libcurl` or a lightweight HTTP lib. Reads model name from `config/game.yaml`.

**`PromptBuilder`** — Takes `GameState` + personality `.md` file content → formats a complete prompt including: known cards, community cards, pot size, bet history, chip counts, and personality instructions. Returns string.

**`ClaudeClient`** — Stubbed for future use. Implements `ILLMClient`, targets Anthropic REST API.

### UI Layer

**`SetupScreen`** — First screen shown on launch. SFML form for: number of AI players (1–8), starting chip stacks, small/big blind amounts. Submits to `main.cpp` to construct the game.

**`GameRenderer`** — Reads `GameState` each frame, draws: table felt, community cards, player hands, chip counts, pot, current bet indicators, "thinking..." overlay when AI is active.

**`InputHandler`** — Maps SFML events to `Action` values. Fold/Call/Raise buttons + raise amount input.

---

## Data Flow

```
main.cpp
  └── reads config/game.yaml
  └── shows SetupScreen → collects PlayerConfig
  └── constructs GameEngine with vector<IPlayer*>
  └── game loop:
        GameEngine::tick()
          └── requests Action from active IPlayer
                HumanPlayer → blocks on InputHandler
                AIPlayer → PromptBuilder → OllamaClient → parse → Action
          └── updates GameState
          └── GameRenderer::render(GameState)
```

---

## Configuration

**`config/game.yaml`** — Advanced/technical settings:
```yaml
llm:
  backend: ollama           # ollama | claude | openai (future)
  model: llama3.2
  endpoint: http://localhost:11434

hand_evaluator:
  library: poker-hand-evaluator   # or "custom" for self-implementation exercise

logging:
  level: info               # debug | info | warn | error
```

**`config/personalities/*.md`** — Plain markdown files injected into AI prompts. Contains personality description, play style instructions, example reasoning. Editable without recompiling.

**Startup UI (SetupScreen)** — Player count, starting stacks, blind amounts. No config file needed.

---

## Error Handling

Minimal for now, designed for extension:
- LLM call failure → AI player defaults to `Action::Call` (safe fallback)
- Malformed LLM response → retry once, then fallback to `Call`
- `ILLMClient` interface makes it easy to add retry logic, timeouts, or circuit breakers later

---

## Testing Strategy

**Framework:** Catch2 v3 (via CMake FetchContent)

- `tests/core/` — `HandEvaluator` with known hand fixtures, `GameEngine` state transitions, `GameState` mutations
- `tests/players/` — `AIPlayer` with `MockLLMClient` injected via `ILLMClient` interface
- `tests/ai/` — `PromptBuilder` output format correctness, personality injection

No mocking of the deck or game engine in integration tests — test against real logic.

---

## Build System

CMake 3.21+ with `FetchContent` for all dependencies. No manual downloads.

**Dependencies:**
| Library | Purpose |
|---|---|
| SFML 2.6 | Graphics, window, input |
| nlohmann/json | JSON parsing (Ollama API responses) |
| yaml-cpp | Parsing `config/game.yaml` |
| Catch2 v3 | Unit testing |
| PokerHandEvaluator | Hand strength evaluation |

---

## Phased Delivery

| Phase | Deliverable |
|---|---|
| 1 | CMake scaffold, project structure, dependencies fetching |
| 2 | Core layer: Card, Deck, Hand, GameState, GameEngine (no UI) |
| 3 | Core layer tests with Catch2 |
| 4 | AI layer: ILLMClient, OllamaClient, PromptBuilder |
| 5 | Players layer: IPlayer, HumanPlayer, AIPlayer |
| 6 | UI layer: SetupScreen, GameRenderer, InputHandler |
| 7 | Wire everything in main.cpp, full playable game |
| 8 | Personality config files + prompt injection |

---

## Future Extensions (Out of Scope Now)

- Async AI calls (`std::async`, thread pool)
- Web UI frontend (C++ HTTP backend, swap UI layer)
- Additional LLM backends (Claude, OpenAI)
- Event-driven architecture (Approach C)
- Spectator/AI-vs-AI mode
- Hand replay via event stream recording
- Self-implemented hand evaluator (learning exercise with guided templates)
