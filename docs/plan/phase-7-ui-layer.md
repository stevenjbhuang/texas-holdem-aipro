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
