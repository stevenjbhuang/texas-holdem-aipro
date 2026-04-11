# Texas Hold'em AI Pro

A Texas Hold'em game written in C++17 with SFML graphics and LLM-powered AI opponents via pluggable backends (llama.cpp, Ollama, OpenAI-compatible APIs).

## Features

- Full Texas Hold'em rules — blinds, betting rounds, side pots, hand evaluation
- SFML-rendered table with card animations and chip display
- AI players driven by local or remote LLMs, each with a configurable personality
- Swappable LLM backends: llama.cpp, Ollama, or any OpenAI-compatible endpoint
- Human player with interactive action buttons (fold / call / raise)
- Multithreaded: game engine on worker thread, renderer on main thread
- GoogleTest unit and integration test suite

## Requirements

- C++17 compiler (GCC 11+ or Clang 14+)
- CMake 3.21+
- SFML system libraries

```bash
sudo apt install libsfml-dev
```

All other dependencies (GoogleTest, yaml-cpp, nlohmann-json, poker-hand-evaluator) are fetched automatically via CMake FetchContent.

## Build

```bash
# Configure
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build build -j$(nproc)

# Run
./build/texas-holdem

# Clean rebuild
rm -rf build && cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
```

## Configuration

### LLM Backend

Edit `config/game.yaml` to select a backend:

```yaml
llm:
  backend: "config/backends/llamacpp.yaml"   # or ollama.yaml / openai.yaml
```

Each backend file sets the model, endpoint, and timeouts:

| Backend | Default model | Default endpoint |
|---|---|---|
| `llamacpp.yaml` | `gemma-4-31B-it-GGUF:Q4_K_M` | `http://localhost:8080` |
| `ollama.yaml` | `glm-4.7-flash` | `http://localhost:11434` |
| `openai.yaml` | `gpt-4o` | `https://api.openai.com` |

For OpenAI, set your key in `config/backends/openai.yaml` or via the `OPENAI_API_KEY` environment variable.

### AI Personalities

Personalities live in `config/personalities/`. Each `.md` file defines a player's name, style, and system prompt. Included personalities:

- `aggressive.md` — Loose-aggressive (Alex): wide opens, frequent 3-bets, multi-street bluffs
- `default.md` — Tight-aggressive (Brian): GTO-adjacent, balanced ranges, board-texture sizing
- `cautious.md` — Tight-passive (Cautious): value-heavy, minimal bluffing

Add new `.md` files in the same format to create custom personalities.

## Architecture

Four decoupled layers — no cross-layer dependencies:

```
src/
  core/       Pure game logic: types, deck, hand evaluator, game engine, game state
  players/    IPlayer interface, HumanPlayer, AIPlayer
  ai/         ILLMClient interface, OllamaClient/LlamaCppClient, PromptBuilder
  ui/         SFML renderer, input routing
```

- `GameEngine` runs on a **worker thread**; SFML renders on the **main thread**
- `GameEngine` only knows `IPlayer` — never concrete player types
- `AIPlayer` only knows `ILLMClient` — never a concrete backend
- `HumanPlayer` decouples from SFML via `std::promise<Action>` / `std::future<Action>`

## Tests

```bash
cd build && ctest --output-on-failure
```

Tests cover core game logic (no mocks) and AI player behaviour (`MockLLMClient` via `ILLMClient`).

## Project Structure

```
assets/          Card, chip, font, sound, and table assets
cmake/           CMake dependency helpers
config/          game.yaml, backends/, personalities/
src/             Source code (core / players / ai / ui)
tests/           GoogleTest suites
```
