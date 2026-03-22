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

- [ ] Create `src/players/IPlayer.hpp` (replace the stub from Phase 3):

```cpp
#pragma once
#include "../core/Types.hpp"
#include "../core/Hand.hpp"
#include "../core/GameState.hpp"
#include <string>

namespace poker {

class IPlayer {
public:
    virtual ~IPlayer() = default;
    virtual PlayerId    getId()   const = 0;
    virtual std::string getName() const = 0;
    virtual void        dealHoleCards(const Hand& cards) = 0;
    virtual Action      getAction(const GameState& state) = 0;
};

} // namespace poker
```

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
    Action      getAction(const GameState& state) override;

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
  - `getAction()`: replace `m_promise` with a fresh `std::promise<Action>` FIRST (before setting the flag), then store the new `future`, set `m_waitingForInput = true`, call `m_future.get()` (blocks), reset flag, return action
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
    Action      getAction(const GameState& state) override;

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
  - `getAction()`: call `PromptBuilder::build(state, m_id, m_personalityText)`, send to `m_client.sendPrompt()`, parse result
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
