# Phase 9 — Polish & AI Tuning

> **Status: IN PROGRESS** — Tasks 9.2, 9.4 complete; Task 9.3 partially complete (animations done, card sprites not yet). Tasks 9.5–9.6 added during this phase.

**This phase is open-ended — explore at your own pace.**

**Previous phase:** [Phase 8 — Wire It All Together](phase-8-wire-up.md)

---

### Task 9.1: Tune the AI prompt

- [ ] Play a few hands. Does the AI make sensible decisions?
- [ ] Iterate on `config/personalities/default.md` and `PromptBuilder.cpp`
- [ ] Try different Ollama models: `llama3.2`, `mistral`, `phi3`

---

### Task 9.2: Add personality variety ✅

- [x] Assign different personality files to each AI player (via SetupScreen personality selector)
- [x] Add new personalities: `aggressive.md` and `cautious.md` in `config/personalities/`
- [ ] Observe how different personalities affect the game

---

### Task 9.3: Improve the renderer ✅ (partial)

- [ ] Add real card sprites (download a free card asset set to `assets/cards/`)
- [ ] Show action history log on screen
- [x] Animate chip movements — `AnimationManager` with linear interpolation, `FadePanel`; chip sprites in `assets/chips/`; table texture in `assets/table/`; sound effects in `assets/sounds/` (deal, chip, fold, win)

---

### Task 9.4: Add a raise input widget ✅

- [x] Add a stepper input for the raise amount in `InputHandler`
- [x] Clamp to `[minRaise, playerStack]`

---

### Task 9.5: Multi-backend LLM config ✅

Completed ahead of Phase 11 schedule.

- [x] Replace `OllamaClient` with `OpenAICompatibleClient` (works with Ollama, llama.cpp, OpenAI, LM Studio)
- [x] Split config into `config/game.yaml` (top-level) and `config/backends/*.yaml` (per-backend)
  - `config/backends/llamacpp.yaml`, `ollama.yaml`, `openai.yaml` provided
  - Switch backends by changing one line in `game.yaml`
- [x] API key support: yaml value or `OPENAI_API_KEY` env var fallback
- [x] Enable HTTPS via `HTTPLIB_USE_OPENSSL` in `cmake/Dependencies.cmake`

Phase 11 Task 11.2 (`OpenAICompatibleClient`) is now **DONE** — no further work needed there.

---

### Task 9.6: LLM Debug Panel ✅

- [x] `LLMDebugLog` — thread-safe ring buffer (4 entries), also writes to `logs/debug_llm.log`
- [x] `DebugPanel` — SFML side panel showing last 4 LLM request/response pairs, scrollable
- [x] Wired via `onPromptComplete` callback on `OpenAICompatibleClient`
- [x] Enabled only in debug builds (`#ifndef NDEBUG`) and when `debug.panel: true` in `config/game.yaml`

---

## Evaluation

Run `/phase-verify` to compile and get automated feedback on your implementation.

### Build checklist

```bash
# 1. Build — must be clean after every polish task you complete
cmake --build build -j$(nproc)
# Success: zero errors; "Built target texas-holdem".

# 2. All tests still pass
cd build && ctest --output-on-failure
# Success: exit code 0. Polish changes must not regress core logic.

# 3. Run the game end-to-end
./build/texas-holdem
# Success checklist (tick off each as you verify it):
#   - SetupScreen appears and accepts different player counts
#   - At least one non-default AI personality is in play (e.g. maniac.md)
#   - Community cards render with real sprites OR labeled colored rectangles (your choice)
#   - Action history is visible on screen during a hand
#   - Raise widget clamps input to [minRaise, playerStack] — entering 0 or a huge number is rejected
#   - Closing the window mid-hand does not crash or hang
```

Because this phase is open-ended, "success" is whatever subset of tasks you chose to complete — but each completed task must not break previously working features.

### Concept checklist

- [ ] When I switched to real card sprites, did I load each `sf::Texture` once (as a member or in a cache) rather than reloading from disk every frame?
- [ ] Does the action history log read from `GameState` (data the engine already tracks) rather than being maintained as a separate UI-side list that could go out of sync?
- [ ] When adding a new personality file, did I load it via the same `ILLMClient` path as the default personality — no hardcoded strings in `AIPlayer`?
- [ ] Does the raise widget clamp the value using `std::clamp(amount, minRaise, playerStack)` (or equivalent) before calling `humanPlayer.provideAction()`?
- [ ] Did I add any new `.cpp` files to the appropriate `CMakeLists.txt` (not just create the file and wonder why it was ignored by the build)?
- [ ] If I animated chip movements, is the animation driven by elapsed time (`sf::Clock` / delta-time) rather than a fixed frame counter that would break at different frame rates?

### Common mistakes

| Mistake | Symptom | Fix |
|---|---|---|
| Loading `sf::Texture` inside `GameRenderer::render()` on every frame | Severe frame rate drop (< 5 fps), high disk I/O, possible crashes from texture going out of scope | Load all textures once in `GameRenderer`'s constructor or an `init()` method; store them as member variables |
| Maintaining action history only in the UI layer (not in `GameState`) | History disappears on reconnect or snapshot; history and actual game events diverge | Store action history as `std::vector<std::string> actionLog` inside `GameState`; append there in `GameEngine::tick()` |
| Hardcoding the raise slider max to `1000` instead of the player's actual stack | Player can raise more chips than they own, corrupting stack accounting | Read `state.players[humanIndex].stack` at the moment the slider is rendered and use that as the upper bound |
| Adding a new `.cpp` file without updating `CMakeLists.txt` | The new file compiles in isolation if you open it in an IDE but is silently excluded from the build; linker errors when the symbol is referenced | Add every new source file to the `add_library(...)` or `add_executable(...)` call in the relevant `CMakeLists.txt` |

### Self-score

- **Solid**: completed 3 or more polish tasks cleanly, all tests pass, no regressions, textures loaded once, raise widget properly clamped.
- **Learning**: completed 1–2 tasks, hit one or two of the common mistakes above and fixed them, game still fully playable.
- **Needs review**: regressions in Phase 8 features (threading, mutex), textures reloaded every frame causing unplayable frame rate, or raise widget allows invalid bets — revisit the relevant concept before shipping.
