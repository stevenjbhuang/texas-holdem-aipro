# Phase 8 — Wire It All Together in `main.cpp`

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
