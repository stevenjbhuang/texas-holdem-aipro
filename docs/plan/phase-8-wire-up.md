# Phase 8 — Wire It All Together in `main.cpp`

> **Status: COMPLETE** (commit `f64bf83`) — main.cpp wires all layers: yaml-cpp config loading, OllamaClient, players, GameEngine on worker thread, SFML game loop. Game is fully playable.

**Concepts you'll learn:** Dependency wiring, threading with `std::thread`, RAII for thread lifetime, config loading with yaml-cpp.

**Previous phase:** [Phase 7 — UI Layer](phase-7-ui-layer.md)
**Next phase:** [Phase 9 — Polish & AI Tuning](phase-9-polish.md)

---

### Task 8.1: Load config from `game.yaml`

- [ ] Link `yaml-cpp` into the main executable in `src/CMakeLists.txt`:

```cmake
target_link_libraries(texas-holdem PRIVATE
    core ai players ui
    yaml-cpp          # FetchContent target name (not yaml-cpp::yaml-cpp)
)
```

- [ ] In `main.cpp`, read `config/game.yaml` using yaml-cpp:

```cpp
#include <yaml-cpp/yaml.h>

YAML::Node config = YAML::LoadFile("config/game.yaml");
std::string model    = config["llm"]["model"].as<std::string>();
std::string endpoint = config["llm"]["endpoint"].as<std::string>();
```

**Your turn:** Extract all needed config values: model, endpoint.

---

### Task 8.2: Construct all objects

- [ ] In `main.cpp`, construct in this order:
  1. `OllamaClient` with model + endpoint from config
  2. Load personality text from `config/personalities/default.md`
  3. Create `HumanPlayer` (id=0, name="You")
  4. Create N `AIPlayer` instances (configurable from `SetupScreen`)
  5. Build `vector<unique_ptr<IPlayer>>`
  6. Create `GameEngine` with players + config
  7. Create SFML `sf::RenderWindow`
  8. Create `GameRenderer`, `InputHandler`, `SetupScreen`

---

### Task 8.3: Run `GameEngine` on a worker thread

**Concept:** `std::thread` runs a function on a new OS thread. We must join it before `main()` exits to avoid undefined behavior.

```cpp
std::thread engineThread([&engine]() {
    while (!engine.isGameOver()) {
        engine.tick();
    }
});

// ... main SFML loop runs on this thread ...

engineThread.join();  // wait for engine to finish before exiting
```

**Your turn:** Wrap this correctly with exception safety. What happens if the engine thread throws? How do you handle that?

---

### Task 8.4: Implement the main game loop

- [ ] Wire up the full game loop:

```cpp
while (window.isOpen() && !engine.isGameOver()) {
    sf::Event event;
    while (window.pollEvent(event)) {
        if (event.type == sf::Event::Closed) window.close();
        inputHandler.handle(event);
    }

    GameState snapshot = engine.getStateSnapshot();  // thread-safe copy
    window.clear(sf::Color(34, 100, 34));             // poker green
    renderer.render(snapshot);
    window.display();
}
```

- [ ] Implement `GameEngine::getStateSnapshot()` — lock `m_stateMutex`, copy `m_state`, unlock, return copy. Also wrap every write to `m_state` inside `GameEngine::tick()` with the same mutex lock so reads and writes are always synchronized.

- [ ] Build and run:
```bash
cmake --build build -j$(nproc)
./build/texas-holdem
```

Expected: SetupScreen appears, after configuration the game starts with cards dealt and AI players taking turns.

- [ ] Commit:
```bash
git add -A && git commit -m "feat: wire up full game loop with threaded GameEngine"
```

---

## Evaluation

Run `/phase-verify` to compile and get automated feedback on your implementation.

### Build checklist

```bash
# 1. Configure (if CMakeLists.txt changed)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug

# 2. Build — expect zero errors
cmake --build build -j$(nproc)
# Success: "Built target texas-holdem" with no undefined reference errors.

# 3. Run all tests — engine threading must not break existing tests
cd build && ctest --output-on-failure
# Success: exit code 0, all suites pass.

# 4. Run the game
./build/texas-holdem
# Success: SetupScreen appears in a window.
#          After clicking Start, the table renders with player names and chip counts.
#          AI players show "Thinking..." while the LLM is queried.
#          Human player sees Fold/Call/Raise buttons on their turn.
#          No crash, no deadlock after 3+ full hands.
```

If Ollama is not running locally, `OllamaClient` should return a fallback action (fold or check) rather than hanging the engine thread indefinitely. Verify this by stopping Ollama and confirming the game still progresses.

### Concept checklist

- [ ] Does `main.cpp` construct objects in dependency order — `OllamaClient` before `AIPlayer`, players before `GameEngine`, engine before the SFML window?
- [ ] Is `engineThread.join()` called unconditionally (not only on the happy path), so the thread is always cleaned up before `main()` exits?
- [ ] Is every write to `m_state` inside `GameEngine::tick()` protected by `m_stateMutex`, and does `getStateSnapshot()` lock the same mutex before copying?
- [ ] Is `getStateSnapshot()` returning a **value** (a full copy of `GameState`), not a reference or pointer into the engine's internal state?
- [ ] Does the main game loop call `window.pollEvent()` rather than `window.waitEvent()`, so the UI stays responsive while the engine thread is blocked waiting for the AI?
- [ ] Did I link `yaml-cpp` (not `yaml-cpp::yaml-cpp`) to the `texas-holdem` target as noted in `CLAUDE.md`?
- [ ] If `config/game.yaml` is missing or malformed, does the program give a readable error message rather than a silent crash or `std::bad_cast`?

### Common mistakes

| Mistake | Symptom | Fix |
|---|---|---|
| Calling `engineThread.join()` only in the happy path (e.g., inside `if (!engine.isGameOver())`) | Thread is abandoned on window close; program hangs at exit or terminates with `std::terminate` | Use RAII: wrap the thread in a guard, or place `join()` after the game loop unconditionally — ensure the engine's stop flag is set before joining |
| Reading `m_state` in `getStateSnapshot()` without locking `m_stateMutex` | Intermittent rendering glitches, torn card/chip values, or data races flagged by `-fsanitize=thread` | Lock `m_stateMutex` in `getStateSnapshot()` for the entire duration of the copy |
| Using `yaml-cpp::yaml-cpp` as the CMake link target instead of `yaml-cpp` | CMake configure error: `CMake Error: target 'yaml-cpp::yaml-cpp' not found` | Use the plain target name `yaml-cpp` as documented in `CLAUDE.md` and the phase task |
| Constructing `sf::RenderWindow` before entering the main-thread loop (e.g., on a background thread or in a helper constructor) | Crash or blank window on Linux; SFML OpenGL context must be created on the thread that will call `display()` | Keep window construction and all `window.*` calls on the main thread |
| Not setting an engine stop flag before calling `join()` when the window closes early | `join()` blocks forever because the engine thread is waiting on `IPlayer::getAction()` | Add a `std::atomic<bool> m_stopRequested` to `GameEngine`; check it in `tick()` and signal it before `join()` |

### Self-score

- **Solid**: game launched first try, cards dealt, AI takes turns, human buttons work, `join()` is unconditional, mutex guards all state access.
- **Learning**: needed 1–2 fixes (wrong yaml-cpp target, forgot to join on early exit), game fully playable after corrections.
- **Needs review**: persistent deadlock, data race warnings from the sanitizer, or game crashes mid-hand — revisit the threading model section of `CLAUDE.md` and the mutex concept box before moving on.

---

## Reference: Build & Test Commands

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
