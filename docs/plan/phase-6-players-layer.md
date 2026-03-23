# Phase 6 — Players Layer: IPlayer, HumanPlayer, AIPlayer

**Concepts you'll learn:** Pure virtual interfaces, `std::promise`/`std::future` for thread communication, `std::atomic`, dependency injection.

**Previous phase:** [Phase 5 — AI Layer](phase-5-ai-layer.md)
**Next phase:** [Phase 7 — UI Layer](phase-7-ui-layer.md)

---

### Task 6.1: Understand `std::promise` and `std::future`

**Concept:** These are two halves of a one-shot communication channel across threads.

```cpp
std::promise<int> promise;
std::future<int> future = promise.get_future();

// On thread A (waits for value):
int value = future.get();  // blocks until promise is fulfilled

// On thread B (provides value):
promise.set_value(42);     // unblocks thread A
```

In our case: `HumanPlayer::getAction()` (on GameEngine thread) blocks on `future.get()`. `InputHandler` (on main/UI thread) calls `promise.set_value(action)` when the user clicks a button.

**Your turn:** What happens if `promise.set_value()` is never called? What does the blocking thread do?

---

### Task 6.2: Implement `IPlayer`

- [ ] Replace the Phase 3 stub at `src/players/IPlayer.hpp` with the full interface. The stub already has `getId()`, `dealHoleCards()`, and `getAction(const PlayerView&)` — add `getName()` and the missing includes:

```cpp
#pragma once
#include "../core/Types.hpp"
#include "../core/Hand.hpp"
#include "../core/PlayerView.hpp"
#include <string>

namespace poker {

class IPlayer {
public:
    virtual ~IPlayer() = default;
    virtual PlayerId    getId()   const = 0;
    virtual std::string getName() const = 0;
    virtual void        dealHoleCards(const Hand& cards) = 0;
    virtual Action      getAction(const PlayerView& view) = 0;
};

} // namespace poker
```

Note the parameter type: `const PlayerView&`, not `const GameState&`. `PlayerView` (introduced in Phase 3, Task 3.2) is the filtered snapshot that exposes only the calling player's own hole cards. Players should never see a `GameState` directly — that would give them access to all opponents' hands.

---

### Task 6.3: Implement `HumanPlayer`

- [ ] Create `src/players/HumanPlayer.hpp`:

```cpp
#pragma once
#include "IPlayer.hpp"
#include <future>
#include <atomic>

namespace poker {

class HumanPlayer : public IPlayer {
public:
    HumanPlayer(PlayerId id, const std::string& name);

    PlayerId    getId()   const override;
    std::string getName() const override;
    void        dealHoleCards(const Hand& cards) override;
    Action      getAction(const PlayerView& view) override;

    // Called by InputHandler on the UI thread to fulfill the action
    void provideAction(Action action);

    // InputHandler checks this to know when to show buttons
    bool isWaitingForInput() const { return m_waitingForInput.load(); }

private:
    PlayerId    m_id;
    std::string m_name;
    Hand        m_hand;

    std::promise<Action>       m_promise;
    std::future<Action>        m_future;
    std::atomic<bool>          m_waitingForInput{false};
    // m_waitingForInput is atomic because InputHandler reads it on the UI thread
    // while GameEngine writes it on the engine thread. Plain bool here is a data race.
};

} // namespace poker
```

- [ ] Implement `src/players/HumanPlayer.cpp`. **Your turn.** Key points:
  - `getAction(const PlayerView& view)`: replace `m_promise` with a fresh `std::promise<Action>` FIRST (before setting the flag), then store the new `future`, set `m_waitingForInput = true`, call `m_future.get()` (blocks), reset flag, return action. The `view` parameter can be stored or ignored for now — `InputHandler` handles the UI side.
  - `provideAction()`: call `m_promise.set_value(action)` — this unblocks `getAction()`
  - **Thread safety:** Replace the promise BEFORE setting `m_waitingForInput`. If `InputHandler` checks the flag and calls `provideAction()` before the new promise is ready, `set_value()` lands on the old promise and `get()` hangs forever. The order matters.

---

### Task 6.4: Implement `AIPlayer`

- [ ] Create `src/players/AIPlayer.hpp`:

```cpp
#pragma once
#include "IPlayer.hpp"
#include "../ai/ILLMClient.hpp"
#include <string>

namespace poker {

class AIPlayer : public IPlayer {
public:
    AIPlayer(PlayerId id, const std::string& name,
             ILLMClient& client, const std::string& personalityText);

    PlayerId    getId()   const override;
    std::string getName() const override;
    void        dealHoleCards(const Hand& cards) override;
    Action      getAction(const PlayerView& view) override;

private:
    Action parseResponse(const std::string& response) const;
    Action fallbackAction() const;

    PlayerId    m_id;
    std::string m_name;
    ILLMClient& m_client;  // reference, not owner
    std::string m_personalityText;
    Hand        m_hand;
};

} // namespace poker
```

- [ ] Implement `src/players/AIPlayer.cpp`. **Your turn.** Key points:
  - `getAction(const PlayerView& view)`: call `PromptBuilder::build(view, m_personalityText)`, send to `m_client.sendPrompt()`, parse result. `PromptBuilder` receives `PlayerView` (not `GameState`) so it only has access to what the player should see — opponent hands are structurally absent.
  - `parseResponse()`: look for "FOLD", "CALL", or "RAISE <amount>" in the response string
  - On empty response or parse failure: retry once, then return `fallbackAction()` which returns `Action::Call`

- [ ] Create `src/players/CMakeLists.txt`:

```cmake
add_library(players STATIC
    HumanPlayer.cpp
    AIPlayer.cpp
)
target_include_directories(players PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(players PUBLIC core ai)
```

- [ ] Add `add_subdirectory(players)` to `src/CMakeLists.txt`

---

### Task 6.5: AIPlayer tests with MockLLMClient

- [ ] Create `tests/players/MockLLMClient.hpp`:

```cpp
#pragma once
#include "../../src/ai/ILLMClient.hpp"

class MockLLMClient : public poker::ILLMClient {
public:
    std::string responseToReturn = "CALL";

    std::string sendPrompt(const std::string&) override {
        return responseToReturn;
    }
};
```

- [ ] Create `tests/players/CMakeLists.txt` and `tests/players/test_ai_player.cpp`

- [ ] **Your turn:** Write tests for:
  - When mock returns `"FOLD"`, `getAction()` returns `Action::Type::Fold`
  - When mock returns `"RAISE 200"`, action type is Raise and amount is 200
  - When mock returns garbage, AIPlayer falls back to Call

- [ ] Uncomment `add_subdirectory(players)` in `tests/CMakeLists.txt`

- [ ] Build and run tests:
```bash
cmake --build build -j$(nproc) && cd build && ctest --output-on-failure
```

- [ ] Commit:
```bash
git add -A && git commit -m "feat(players): add IPlayer, HumanPlayer, AIPlayer with tests"
```

---

## Evaluation

Run `/phase-verify` to compile and get automated feedback on your implementation.

### Build checklist

```bash
# 1. Configure (if not already done, or after adding new CMakeLists files)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug

# 2. Build all libraries and tests
cmake --build build -j$(nproc)

# 3. Run all tests including the new AIPlayer tests
cd build && ctest --output-on-failure
```

Success looks like:
- `cmake --build` exits with code `0`; both `libplayers.a` and the `players_tests` binary are produced
- `ctest` output shows the new `AIPlayer` tests alongside the Phase 4 core tests, all passing:
  ```
  Test #1: AIPlayerTest.FoldResponse ........ Passed
  Test #2: AIPlayerTest.RaiseResponse ....... Passed
  Test #3: AIPlayerTest.GarbageFallback ..... Passed
  ```
- `100% tests passed, 0 tests failed`

Verify the threading behaviour separately: create a small manual test where `HumanPlayer::getAction` is called on one thread and `provideAction` is called from another after a short delay — confirm the call unblocks and returns the correct action.

### Concept checklist

- [ ] Do both `HumanPlayer` and `AIPlayer` declare `getAction(const PlayerView& view)` (not `const GameState&`)?
- [ ] Does `HumanPlayer::getAction()` create a brand-new `std::promise<Action>` and store a fresh `std::future<Action>` each time it is called (not reusing the old ones)?
- [ ] In `HumanPlayer::getAction()`, is the promise replaced BEFORE `m_waitingForInput` is set to `true`?
- [ ] Is `m_waitingForInput` declared as `std::atomic<bool>` (not plain `bool`)?
- [ ] Does `AIPlayer` hold `ILLMClient` as a reference (`ILLMClient&`), not by value or `unique_ptr`?
- [ ] Does `AIPlayer::getAction()` call `fallbackAction()` on an empty response AND on a parse failure (not only on empty)?
- [ ] Does `parseResponse()` handle case variations or leading/trailing whitespace in the LLM response?
- [ ] Does `MockLLMClient` inherit from `poker::ILLMClient` (with the namespace), not a locally defined copy?
- [ ] Is `add_subdirectory(players)` uncommented in both `src/CMakeLists.txt` and `tests/CMakeLists.txt`?

### Common mistakes

| Mistake | Symptom | Fix |
|---|---|---|
| `getAction` signature takes `const GameState&` instead of `const PlayerView&` | Compile error: `IPlayer::getAction` override has wrong signature; the abstract method is not overridden | Change the parameter type to `const PlayerView& view` in both the `.hpp` declaration and `.cpp` definition |
| Reusing the same `std::promise` across multiple `getAction()` calls | Second call to `getAction()` throws `std::future_error: Promise already satisfied` at runtime | Reassign `m_promise = std::promise<Action>{}` and `m_future = m_promise.get_future()` at the top of each `getAction()` call |
| Setting `m_waitingForInput = true` before replacing the promise | Race condition: `provideAction()` can call `set_value()` on the old promise; `get()` on the new future hangs forever | Always replace the promise and assign the future BEFORE setting the atomic flag |
| Storing `ILLMClient` by value in `AIPlayer` | Compile error: cannot instantiate abstract class | Declare the member as `ILLMClient& m_client` (reference); ownership stays with the caller |
| `parseResponse()` only checks for exact uppercase strings | LLM returns `"fold"` or `"Raise 150"` and AIPlayer always falls back to Call | Normalize the response to uppercase with `std::transform` before pattern matching |
| Forgetting `add_subdirectory(players)` in `tests/CMakeLists.txt` | `players_tests` target is never defined; `ctest` finds no new tests | Uncomment the line in `tests/CMakeLists.txt` and re-run CMake configure |

### Self-score

- **Solid**: All tests green on first build. You can explain why a stale promise causes a deadlock and why `m_waitingForInput` must be atomic. Every checklist box checked without hesitation.
- **Learning**: Built after 1-2 fixes — typically the promise reuse bug or a missing `add_subdirectory`. You re-read Task 6.1 to understand the promise/future ordering requirement. Most checklist items clear after that.
- **Needs review**: Deadlock in manual threading test, multiple linker errors, or AIPlayer always falls back to Call even for valid responses. Revisit the concept boxes in Tasks 6.1 and 6.4, trace through the promise replacement sequence on paper, and check your `parseResponse()` logic against each of the three mock response values before continuing to Phase 7.
