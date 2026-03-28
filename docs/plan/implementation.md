# Texas Hold'em AI Pro — Implementation Plan

> **This is a guided learning plan.** Each phase introduces a concept, shows you a scaffold, and asks you to implement it. Only ask for the answer if you're stuck. The goal is that *you* write the code.

**Goal:** Build a playable Texas Hold'em game in C++ with SFML graphics and LLM-powered AI players via Ollama.

**Architecture:** Three decoupled layers (core / players / ai / ui) communicating through well-defined interfaces. `GameEngine` runs on a worker thread; SFML renders on the main thread.

**Tech Stack:** C++17, CMake 3.21+, SFML 2.6, cpp-httplib, nlohmann/json, yaml-cpp, GoogleTest, PokerHandEvaluator, Ollama (local LLM)

---

## Phases

| Phase | Topic | Concepts |
|---|---|---|
| [1](phase-1-cmake-scaffold.md) | CMake Scaffold & Project Structure | CMake, FetchContent, out-of-source builds |
| [2](phase-2-core-types-card-deck-hand.md) | Core Layer: Types, Card, Deck, Hand | Value types, enum class, operator overloading, PRNG |
| [3](phase-3-gamestate-gameengine.md) | Core Layer: GameState & GameEngine | State machines, STL containers, const-correctness, mutex |
| [4](phase-4-core-tests.md) | Core Tests with GoogleTest | Unit testing, test design, mock objects |
| [5](phase-5-ai-layer.md) | AI Layer: ILLMClient, OllamaClient, PromptBuilder | Interfaces, HTTP clients, JSON, prompt engineering |
| [6](phase-6-players-layer.md) | Players Layer: IPlayer, HumanPlayer, AIPlayer | Pure virtual interfaces, std::promise/future, std::atomic |
| [7](phase-7-ui-layer.md) | UI Layer: SFML Setup, Renderer, Input | Game loop, SFML basics, thread-safe rendering |
| [8](phase-8-wire-up.md) | Wire It All Together | Dependency wiring, std::thread, RAII, config loading |
| [9](phase-9-polish.md) | Polish & AI Tuning | Prompt tuning, personalities, renderer polish |

---

## Build & Test Quick Reference

```bash
# Configure (first time or after CMakeLists changes)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build build -j$(nproc)

# Run tests
cd build && ctest --output-on-failure

# Run the game
./build/texas-holdem

# Clean build
rm -rf build && cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
```

---

## Related docs

- [Design Spec](../spec/design.md) — architecture, components, data flow
- [Guides](../guides/) — per-topic learning guides added as we build each phase
  - [Pointers & smart pointers](../guides/pointers.md)
  - [Interfaces & virtual dispatch](../guides/interfaces.md)
  - [CMake intro](../guides/cmake-intro.md)
  - [Mutexes & variable lifetimes](../guides/mutex-and-lifetimes.md)
