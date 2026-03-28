# Texas Hold'em AI Pro

A C++17 Texas Hold'em game with SFML graphics and LLM-powered AI players via Ollama.

## Learning Context

**This is a guided learning project.** The developer is learning:
- Modern C++17 (interfaces, smart pointers, RAII, STL, threading)
- CMake + FetchContent
- SFML for graphics
- Ollama/LLM API integration

**Do NOT implement things for the user unprompted.** Explain concepts first, provide scaffolds, let them write the code. Only provide full implementations when they are stuck or explicitly ask.

## Build Commands

```bash
# Configure (run once, or after CMakeLists changes)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build build -j$(nproc)

# Run tests
cd build && ctest --output-on-failure

# Run game
./build/texas-holdem

# Clean rebuild
rm -rf build && cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
```

## Architecture

Three decoupled layers — never introduce cross-layer dependencies:

```
ui/          → renders GameState, routes input
players/     → IPlayer interface, HumanPlayer, AIPlayer
core/        → pure game logic (NO deps on ui/players/ai)
ai/          → ILLMClient interface, OllamaClient, PromptBuilder
```

- `GameEngine` runs on a **worker thread**; SFML renders on the **main thread**
- `GameEngine` only knows `IPlayer` — never `HumanPlayer` or `AIPlayer` directly
- `AIPlayer` only knows `ILLMClient` — never `OllamaClient` directly
- `HumanPlayer` decoupled from SFML via `std::promise<Action>` / `std::future<Action>`

## Code Style

- Namespace: `poker::`
- Member variables: `m_name` prefix
- Headers: `#pragma once`
- Smart pointers: prefer `std::unique_ptr`, avoid raw `new/delete`
- No `using namespace std` in headers
- C++17 standard (`-std=c++17`, extensions off)

## Key Files

| File | Purpose |
|---|---|
| `src/core/Types.hpp` | `PlayerId` (opaque identity), `Action`, `Street`, `Suit`, `Rank` |
| `src/core/GameState.hpp` | Central game state — includes `waitingForAction` (renderer signal) and `totalContributed` (side pots) |
| `src/core/GameEngine.hpp` | State machine — internal `SeatIndex`/`m_seats`/`m_seatOf` for rotation; `m_handStarted` lifecycle flag |
| `src/players/IPlayer.hpp` | Interface all players implement |
| `src/ai/ILLMClient.hpp` | Interface for LLM backends |
| `src/ai/PromptBuilder.hpp` | Serializes `GameState` → prompt string |
| `config/game.yaml` | LLM model, endpoint, advanced settings |
| `config/personalities/*.md` | AI player personality prompts |

## CMake Notes

- All deps via `FetchContent` — no manual installs (except SFML system libs)
- SFML needs: `sudo apt install libsfml-dev` for system OpenGL/freetype
- `yaml-cpp` FetchContent target is `yaml-cpp` (NOT `yaml-cpp::yaml-cpp`)
- `poker_hand_evaluator` is a manually wrapped C library (see `cmake/Dependencies.cmake` + Task 1.5)
- Add new source files to the relevant `CMakeLists.txt` in each layer's directory

## Threading Model

- `GameEngine::tick()` runs on a worker thread — drives the game loop
- SFML event loop + `GameRenderer::render()` run on the main thread
- `GameEngine::getStateSnapshot()` returns a mutex-protected copy of `GameState`
- `m_stateMutex` is acquired granularly inside `runBettingRound()` — released around each `getAction()` call so the renderer can read a fresh snapshot between every individual action
- `GameState::waitingForAction` signals to the renderer that the engine is blocked on a player's input — used to drive "Thinking..." / action button overlays
- `HumanPlayer` decouples from SFML via `std::promise<Action>` / `std::future<Action>` (implemented Phase 6)

## Testing

- Framework: GoogleTest (GTest + GMock)
- `tests/core/` — unit tests for pure game logic (no mocks needed)
- `tests/players/` — `AIPlayer` with `MockLLMClient` injected via `ILLMClient`
- `tests/core/MockPlayer.hpp` — mock `IPlayer` for `GameEngine` tests

## Docs

- `docs/spec/design.md` — full architecture and component design
- `docs/plan/implementation.md` — phase-by-phase guided plan index
- `docs/plan/phase-N-*.md` — individual phase files
