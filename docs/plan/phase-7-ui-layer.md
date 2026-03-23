# Phase 7 — UI Layer: SFML Setup, Renderer, Input

**Concepts you'll learn:** Game loop pattern, SFML basics, rendering from shared state, thread-safe state snapshots.

**Previous phase:** [Phase 6 — Players Layer](phase-6-players-layer.md)
**Next phase:** [Phase 8 — Wire It All Together](phase-8-wire-up.md)

---

### Task 7.1: Understand the SFML game loop

**Concept:** A game loop runs continuously:
1. Poll events (keyboard, mouse, window close)
2. Update game state
3. Render

In our design, step 2 happens on the GameEngine thread. The main thread only does steps 1 and 3.

```cpp
while (window.isOpen()) {
    // 1. Poll events
    sf::Event event;
    while (window.pollEvent(event)) {
        if (event.type == sf::Event::Closed) window.close();
        inputHandler.handle(event);  // routes to HumanPlayer if it's their turn
    }

    // 3. Render (reads a snapshot of game state)
    GameState snapshot = engine.getStateSnapshot();  // mutex-protected copy
    window.clear();
    renderer.render(snapshot);
    window.display();
}
```

---

### Task 7.2: Implement `SetupScreen`

- [ ] Create `src/ui/SetupScreen.hpp` and `SetupScreen.cpp`

The setup screen shows before the game starts. It lets the user configure:
- Number of AI players (1–8)
- Starting stack
- Small blind / big blind

Returns a `GameConfig` struct when the user clicks "Start".

**Your turn:** Implement this using SFML's `sf::Text`, `sf::RectangleShape` for buttons, and `sf::Event` for input. Keep it simple — plain buttons and text input fields are enough.

---

### Task 7.3: Implement `GameRenderer`

- [ ] Create `src/ui/GameRenderer.hpp` and `GameRenderer.cpp`

**Your turn:** Implement `render(const GameState& state)`. Start minimal:
1. Draw the table background (green rectangle)
2. Draw each player's name and chip count as text
3. Draw community cards as labeled rectangles (e.g. "A♠")
4. Draw pot size
5. Draw the human player's hole cards
6. Draw "Thinking..." overlay when AI is active (use a flag passed in)

Don't worry about card sprites yet — use colored rectangles with text. You can add real card art later.

---

### Task 7.4: Implement `InputHandler`

- [ ] Create `src/ui/InputHandler.hpp` and `InputHandler.cpp`

The input handler:
- Holds a reference to `HumanPlayer`
- Draws Fold/Call/Raise buttons when `humanPlayer.isWaitingForInput()` is true
- On button click, calls `humanPlayer.provideAction(action)`

**Your turn:** Implement button hit-testing (check if mouse click coordinates are inside button rectangle) and route to `HumanPlayer`.

- [ ] Create `src/ui/CMakeLists.txt`:

```cmake
add_library(ui STATIC
    SetupScreen.cpp
    GameRenderer.cpp
    InputHandler.cpp
)
target_include_directories(ui PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(ui PUBLIC core players sfml-graphics sfml-window sfml-system)
```

- [ ] Add `add_subdirectory(ui)` to `src/CMakeLists.txt`

- [ ] Build:
```bash
cmake --build build -j$(nproc)
```

- [ ] Commit:
```bash
git add -A && git commit -m "feat(ui): add SetupScreen, GameRenderer, InputHandler"
```

---

## Evaluation

Run `/phase-verify` to compile and get automated feedback on your implementation.

### Build checklist

```bash
# 1. Configure (only needed if CMakeLists.txt changed)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug

# 2. Build — expect zero errors, zero warnings about missing symbols
cmake --build build -j$(nproc)
# Success: "Built target ui" appears in output; no undefined reference errors.

# 3. Run tests — ui layer has no unit tests yet, but core/players must still pass
cd build && ctest --output-on-failure
# Success: all previously passing tests still pass (exit code 0).
```

The binary `texas-holdem` will link but produce a blank window or crash at this stage — that is expected until Phase 8 wires the engine. The goal here is a clean compile with the `ui` static library built successfully.

### Concept checklist

Answer these honestly about your own code before moving on.

- [ ] Did I keep all `sf::RenderWindow` and `sf::Event` calls inside the main thread, never inside `GameRenderer` or `InputHandler` constructors that run on other threads?
- [ ] Does `GameRenderer::render()` accept a `const GameState&` (a snapshot copy) rather than a raw pointer or reference to the live engine state?
- [ ] Did I use `sf::RectangleShape` or `sf::Text` for every visual element rather than calling `window.draw()` directly from `SetupScreen`?
- [ ] Does `InputHandler` call `humanPlayer.isWaitingForInput()` before drawing or responding to action buttons?
- [ ] Does `InputHandler` perform proper hit-testing (comparing `event.mouseButton.x/y` against button bounds) rather than using a fixed screen region?
- [ ] Does `SetupScreen::run()` return a fully populated `GameConfig` struct rather than modifying globals?
- [ ] Did I add `SetupScreen.cpp`, `GameRenderer.cpp`, and `InputHandler.cpp` to `src/ui/CMakeLists.txt` and verify they appear under `add_library(ui STATIC ...)`?
- [ ] Did I add `add_subdirectory(ui)` to `src/CMakeLists.txt` so the build system picks up the new library?

### Common mistakes

| Mistake | Symptom | Fix |
|---|---|---|
| Calling `window.draw()` inside `GameRenderer` without passing the window | Linker error: `GameRenderer` has no access to `sf::RenderWindow`, or renderer silently draws nothing | Pass `sf::RenderWindow&` as a parameter to `render()`, or have the caller draw the returned drawables |
| Forgetting `sfml-graphics` in `target_link_libraries` for the `ui` target | Linker errors: undefined reference to `sf::Text`, `sf::RectangleShape`, etc. | Add `sfml-graphics sfml-window sfml-system` to the `target_link_libraries` call in `src/ui/CMakeLists.txt` |
| Drawing action buttons unconditionally instead of checking `isWaitingForInput()` | Buttons appear during AI turns; clicking them corrupts state or calls `provideAction` at the wrong time | Wrap the button draw and click-handling block in `if (m_humanPlayer.isWaitingForInput())` |
| Storing `sf::Font` as a local variable inside a render function | Text renders as blank rectangles or crashes immediately after the function returns | Store `sf::Font` as a member variable of `GameRenderer` — SFML requires the font to outlive the text objects that reference it |
| Using raw `new` for SFML objects | Violates the project's smart-pointer style; potential leaks if an exception is thrown | Use `std::make_unique<sf::RenderWindow>(...)` or plain stack/member variables; avoid bare `new` |

### Self-score

- **Solid**: library built first try, no linker errors, concept questions answered confidently, buttons only appear on the human player's turn.
- **Learning**: needed 1–2 fixes (missing CMake entry, font lifetime bug), re-read one concept box, build clean on second attempt.
- **Needs review**: multiple linker or compile errors, unclear on why SFML calls must stay on the main thread, or action buttons fire during AI turns — revisit the game-loop concept box and the threading model in `CLAUDE.md` before continuing.
