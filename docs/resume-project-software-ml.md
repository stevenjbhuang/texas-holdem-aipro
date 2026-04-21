# Resume Project Entries — Texas Hold'em AI Pro

## Option A: Software Engineer Focus

**Texas Hold'em AI Pro** | C++17, SFML, CMake, GoogleTest, yaml-cpp, cpp-httplib, nlohmann/json

- Built a 4-layer desktop game architecture (`core`, `players`, `ai`, `ui`) with strict interface boundaries (`IPlayer`, `ILLMClient`) to keep game logic decoupled from rendering and model providers.
- Implemented a multithreaded runtime where the game engine ticks on a worker thread and SFML rendering/input run on the main thread, using mutex-protected snapshots and granular locking around player actions.
- Designed and shipped full betting-round/state-machine logic across pre-flop, flop, turn, river, and showdown, including blind rotation, legal action enforcement, all-in handling, and side-pot distribution.
- Integrated OpenAI-compatible inference over HTTP (`/v1/models`, `/v1/chat/completions`) with readiness checks, timeout controls, API-key auth, and backend switching via YAML config for local/cloud model endpoints.
- Added production-style UX polish: setup flow, action controls, event-driven animations, sound effects, fullscreen/resize handling, and a debug panel for prompt/response inspection in non-release builds.
- Established automated quality gates with GoogleTest suites for core engine behavior, AI parsing/prompt generation, and mock-based player/LLM tests; project currently includes 42 source files (~3.6K LOC) and 10 unit tests.

## Option B: Machine Learning Engineer Focus

**Texas Hold'em AI Pro** | C++17, Prompt Engineering, LLM APIs, Evaluation-Oriented Game Simulation

- Built an LLM-driven decision system where poker agents consume structured `PlayerView` snapshots and generate legal actions (`FOLD`, `CHECK/CALL`, `RAISE`) through prompt-conditioned inference.
- Designed `PromptBuilder` to serialize game state, action history, position context, and legal-action constraints into deterministic prompts, with pluggable personality/system prompts loaded from markdown profiles.
- Implemented robust model I/O through an OpenAI-compatible client supporting model discovery, request shaping (`messages`, `max_tokens`), auth headers, timeout tuning, and graceful stop semantics for long-running inference loops.
- Added resilient post-processing in `AIPlayer`: retry-on-empty response, final-line action extraction from verbose model outputs, case-insensitive parsing, malformed-response recovery, and safe fallback actions.
- Enforced information-hiding for fair-play inference by passing filtered per-player views (`makePlayerView`) so opponent private cards are structurally excluded from prompts.
- Created a testable ML integration surface using mock LLM clients and prompt-output assertions to validate decision parsing and prompt correctness without requiring live model calls.

## ATS Keyword Bank

C++, C++17, SFML, multithreading, mutex, state machine, OOP, interface design, dependency inversion, CMake, GoogleTest, YAML, HTTP APIs, OpenAI-compatible API, prompt engineering, LLM integration, model backend abstraction, fault tolerance, test automation.
