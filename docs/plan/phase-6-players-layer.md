# Phase 6 — Players Layer: IPlayer, HumanPlayer, AIPlayer

> **Status: COMPLETE** (commit `0c28c9d`) — IPlayer, HumanPlayer, AIPlayer implemented with tests. All three test suites pass (core_tests, ai_tests, players_tests).

**Concepts you'll learn:** Pure virtual interfaces, `std::promise`/`std::future` for cross-thread communication, `std::atomic`, dependency injection via reference.

**Previous phase:** [Phase 5 — AI Layer](phase-5-ai-layer.md)
**Next phase:** [Phase 7 — UI Layer](phase-7-ui-layer.md)

**Design reminder:** `src/players/` depends on `src/core/` and `src/ai/` — never on `src/ui/`. `GameEngine` only knows `IPlayer`; it never sees `HumanPlayer` or `AIPlayer` directly.

---

### Task 6.1: Update `IPlayer`

The `IPlayer` stub from Phase 3 is missing `getName()`. The `GameRenderer` (Phase 7) uses it to label player seats; `AIPlayer` stores the personality display name.

- [ ] Add `getName()` to `src/players/IPlayer.hpp`:

```cpp
#pragma once
#include "core/Types.hpp"
#include "core/Hand.hpp"
#include "core/PlayerView.hpp"
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

- [ ] Add `getName()` to `tests/core/MockPlayer.hpp` — it implements `IPlayer` and will fail to compile without it:

```cpp
std::string getName() const override { return "MockPlayer"; }
```

<details>
<summary>Concepts</summary>

> **Concept: Adding a pure virtual method to an interface**
>
> When you add a new `= 0` method to `IPlayer`, every concrete subclass becomes abstract until it overrides it. The compiler enforces the contract across the entire codebase at once:
>
> ```
> error: cannot instantiate abstract type 'MockPlayer'
>   note: pure virtual method 'IPlayer::getName()' not overridden
> ```
>
> This is intentional friction — the interface is telling you everywhere the contract has changed. Fix each one and the build is clean again. This is why adding to an interface is a bigger change than adding to a concrete class.

> **Concept: `getName()` vs `getId()`**
>
> `PlayerId` is an opaque integer used as a map key throughout the engine — stable, compact, and used for all data lookups. `getName()` is a human-readable string used only for display. Keeping them separate means the engine never parses a string to do bookkeeping, and the UI never shows a raw integer to the user.

**Questions — think through these before checking answers:**
1. `MockPlayer` is a concrete class used in tests. If you forget to add `getName()`, what error do you get and at which build stage — configure, compile, or link?
2. Why is the return type `std::string` (by value) rather than `const std::string&`?

</details>

<details>
<summary>Answers</summary>

**Q1.** A compile error — when the test source file is compiled into an object. The error reads something like "cannot instantiate abstract type MockPlayer — pure virtual method IPlayer::getName() is not overridden." It is not a linker error because the problem is detected during type-checking, before linking begins.

**Q2.** Returning `const std::string&` binds to a member variable (`m_name`), which is fine for `HumanPlayer` and `AIPlayer`. But for `MockPlayer` — which returns a string literal — there is no member to bind to; you would need a `static` string. Returning by value is the universally safe choice for interface methods: it works regardless of how the subclass stores the name, and compilers elide the copy via NRVO.

</details>

---

### Task 6.2: Implement `HumanPlayer`

`HumanPlayer` bridges two threads. `GameEngine::tick()` runs on a worker thread and calls `getAction()` — which must block until the user clicks a button. The button click arrives on the main (SFML) thread. `std::promise`/`std::future` is the standard C++ solution for this one-shot handoff.

```
Worker thread (GameEngine)        Main thread (InputHandler)
──────────────────────────        ──────────────────────────
HumanPlayer::getAction() called
  m_future.get()  ←── blocks

                                  // user clicks CALL
                                  HumanPlayer::provideAction(action)
                                    m_promise.set_value(action)

  m_future.get() returns ✓
  getAction() returns to engine
```

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

    // Called by InputHandler on the UI thread to unblock getAction()
    void provideAction(Action action);

    // UI thread polls this to know when to show action buttons
    bool isWaitingForInput() const { return m_waitingForInput.load(); }

private:
    PlayerId    m_id;
    std::string m_name;
    Hand        m_hand;

    std::promise<Action>  m_promise;
    std::future<Action>   m_future;
    std::atomic<bool>     m_waitingForInput{false};
};

} // namespace poker
```

- [ ] Implement `src/players/HumanPlayer.cpp`. **Your turn.** Key points:
  - `getAction()`: replace `m_promise` with a fresh `std::promise<Action>` first, get a new `m_future`, then set `m_waitingForInput = true`, then call `m_future.get()`. Reset the flag before returning.
  - `provideAction()`: call `m_promise.set_value(action)` — this unblocks `getAction()`.
  - The promise must be replaced **before** the flag is set. If the flag is set first, `provideAction()` could be called on the old promise before the new one is ready, and `get()` will hang forever.

<details>
<summary>Concepts</summary>

> **Concept: `std::promise` and `std::future` — one-shot channel**
>
> A `promise<T>` is the write end of a one-shot cross-thread channel; a `future<T>` is the read end.
>
> ```cpp
> std::promise<int> p;
> std::future<int>  f = p.get_future();
>
> // Thread A — blocks until value is available:
> int val = f.get();       // hangs here
>
> // Thread B — provides the value:
> p.set_value(42);         // unblocks thread A; f.get() returns 42
> ```
>
> Key rules:
> - `get_future()` may only be called once per `promise`.
> - `set_value()` may only be called once per `promise`.
> - `future::get()` may only be called once — it moves the value out.
>
> Violating any of these throws `std::future_error`. This is why `getAction()` must create a brand-new `promise` at the start of every call.

> **Concept: `std::atomic<bool>` for cross-thread flags**
>
> A plain `bool` is not safe to read and write from two threads simultaneously — that is a data race, which is undefined behaviour in C++. `std::atomic<bool>` makes every load and store indivisible:
>
> ```cpp
> // ❌ Data race — undefined behaviour:
> bool m_waiting = false;
> // Thread A writes: m_waiting = true;
> // Thread B reads: if (m_waiting)  ← UB
>
> // ✓ Safe:
> std::atomic<bool> m_waiting{false};
> m_waiting.store(true);
> if (m_waiting.load()) { ... }
> ```
>
> `std::atomic` has no runtime overhead on x86 — it compiles to a single CPU instruction with the appropriate memory fence. Always use it for flags shared across threads.

> **Concept: Promise replacement ordering**
>
> Inside `getAction()`, the promise must be replaced *before* setting `m_waitingForInput = true`:
>
> ```
> Correct order:
>   1. m_promise = std::promise<Action>{}   ← fresh promise
>   2. m_future  = m_promise.get_future()   ← fresh future
>   3. m_waitingForInput = true             ← signal UI
>   4. m_future.get()                       ← block
>
> Wrong order (race condition):
>   1. m_waitingForInput = true             ← UI sees flag immediately
>      ← UI calls provideAction() → set_value() on the OLD (exhausted) promise
>   2. m_promise = std::promise<Action>{}   ← new promise (value lost!)
>   3. m_future  = m_promise.get_future()
>   4. m_future.get()                       ← blocks FOREVER
> ```
>
> The rule: never signal "I am ready" before the data structure is in place.

> **Concept: Clearing `m_waitingForInput` after `get()` returns**
>
> After `m_future.get()` returns, the engine is no longer waiting for input. Clearing the flag before returning tells the UI to hide the action buttons. If you leave it set, the UI shows buttons during hands where no human input is needed, and clicks on those buttons corrupt the next round's promise.

**Questions — think through these before checking answers:**
1. `m_future.get()` may only be called once per future. How does replacing `m_promise` and `m_future` at the top of each `getAction()` call prevent calling `get()` twice on the same future?
2. Why are `m_promise` and `m_future` stored as member variables rather than local variables inside `getAction()`?

</details>

<details>
<summary>Answers</summary>

**Reference implementation (`HumanPlayer.cpp`):**

```cpp
#include "players/HumanPlayer.hpp"

namespace poker {

HumanPlayer::HumanPlayer(PlayerId id, const std::string& name)
    : m_id(id), m_name(name) {}

PlayerId    HumanPlayer::getId()   const { return m_id; }
std::string HumanPlayer::getName() const { return m_name; }

void HumanPlayer::dealHoleCards(const Hand& cards) {
    m_hand = cards;
}

Action HumanPlayer::getAction(const PlayerView& /*view*/) {
    m_promise = std::promise<Action>{};
    m_future  = m_promise.get_future();

    m_waitingForInput = true;
    Action action = m_future.get();
    m_waitingForInput = false;

    return action;
}

void HumanPlayer::provideAction(Action action) {
    m_promise.set_value(action);
}

} // namespace poker
```

---

**Q1.** Because the old future is destroyed when `m_future` is reassigned. C++ destroys the previous value held by a member variable when it is replaced. By the time `m_future.get()` is called in step 4, `m_future` refers to a brand-new future — no risk of calling `get()` on the same object twice.

**Q2.** `m_promise` must be accessible to `provideAction()`, which is called from the main thread at an arbitrary time after `getAction()` has already blocked. If `m_promise` were a local variable inside `getAction()`, it would live on the worker thread's stack frame — `provideAction()` could not reach it. Member variables persist for the lifetime of the object and are accessible from any method on any thread.

</details>

---

### Task 6.3: Implement `AIPlayer`

`AIPlayer` is stateless per-action: build a prompt, send it to the LLM, parse the response. It holds a reference to `ILLMClient` — the specific backend is injected by the caller.

- [ ] Create `src/players/AIPlayer.hpp`:

```cpp
#pragma once
#include "IPlayer.hpp"
#include "ai/ILLMClient.hpp"
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
    ILLMClient& m_client;          // reference — not an owner
    std::string m_personalityText;
    Hand        m_hand;
};

} // namespace poker
```

- [ ] Implement `src/players/AIPlayer.cpp`. **Your turn.** Key points:
  - `getAction()`: call `PromptBuilder::build(view, m_personalityText)`, send to `m_client.sendPrompt()`, parse the result. On empty response, retry once. On second failure, return `fallbackAction()`.
  - `parseResponse()`: normalise the response to uppercase with `std::transform`. Search for `"FOLD"`, `"CALL"`, `"RAISE"` using `find`. For `"RAISE"`, extract the integer amount that follows.
  - `fallbackAction()`: return `Action{Action::Type::Call}` — safer than folding when chips are already committed.

<details>
<summary>Concepts</summary>

> **Concept: `ILLMClient&` — reference, not owner**
>
> `AIPlayer` receives `ILLMClient` as a reference, not a `unique_ptr`. This means the caller owns the client and is responsible for keeping it alive while the `AIPlayer` exists:
>
> ```cpp
> // ❌ unique_ptr — AIPlayer owns the client:
> class AIPlayer { std::unique_ptr<ILLMClient> m_client; };
>
> // ✓ reference — caller owns the client, AIPlayer just uses it:
> class AIPlayer { ILLMClient& m_client; };
>
> OllamaClient ollama("llama3.2", "http://localhost:11434");
> AIPlayer ai(id, name, ollama, personality);  // ollama must outlive ai
> ```
>
> The reference approach also allows multiple `AIPlayer` instances to share one `OllamaClient` — useful when all AI players talk to the same Ollama instance.

> **Concept: Reference members and copy semantics**
>
> A reference member makes `AIPlayer` non-copyable and non-assignable by default — references cannot be rebound after construction. The compiler implicitly deletes the copy constructor and copy assignment operator. To store `AIPlayer` objects in a container, use `std::unique_ptr<AIPlayer>` or construct in-place with `emplace_back`. This is an expected trade-off: AI players are not meant to be copied.

> **Concept: Normalising LLM responses**
>
> LLMs don't always respect capitalisation instructions. `parseResponse()` should normalise before matching:
>
> ```cpp
> #include <algorithm>
> #include <cctype>
>
> std::string upper = response;
> std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
>
> if (upper.find("FOLD") != std::string::npos) return Action{Action::Type::Fold};
> if (upper.find("CALL") != std::string::npos) return Action{Action::Type::Call};
>
> auto pos = upper.find("RAISE");
> if (pos != std::string::npos) {
>     try {
>         int amount = std::stoi(upper.substr(pos + 6));
>         return Action{Action::Type::Raise, amount};
>     } catch (...) { return fallbackAction(); }
> }
> return fallbackAction();
> ```
>
> Using `find` rather than `==` handles responses like `"- CALL"`, `"I will FOLD."`, or `"RAISE 150 (pot sized)"`.

> **Concept: Retry once, then fall back**
>
> `OllamaClient::sendPrompt` returns `""` on any network or JSON error. Retrying once handles transient failures without hanging the game for multiple round-trips:
>
> ```cpp
> std::string response = m_client.sendPrompt(prompt);
> if (response.empty())
>     response = m_client.sendPrompt(prompt);  // retry once
> return parseResponse(response);
> ```
>
> `parseResponse("")` normalises to `""`, finds no match, and returns `fallbackAction()` — so the empty-string case is already handled there.

**Questions — think through these before checking answers:**
1. `m_client` is a reference member. What does this imply about the copy constructor and copy assignment operator of `AIPlayer`?
2. `parseResponse()` searches for `"RAISE"` and then tries to parse an integer at `pos + 6`. What could go wrong if the response is exactly `"RAISE"` with no amount, and how does the code handle it?

</details>

<details>
<summary>Answers</summary>

**Reference implementation (`AIPlayer.cpp`):**

```cpp
#include "players/AIPlayer.hpp"
#include "ai/PromptBuilder.hpp"
#include <algorithm>
#include <cctype>

namespace poker {

AIPlayer::AIPlayer(PlayerId id, const std::string& name,
                   ILLMClient& client, const std::string& personalityText)
    : m_id(id), m_name(name), m_client(client), m_personalityText(personalityText) {}

PlayerId    AIPlayer::getId()   const { return m_id; }
std::string AIPlayer::getName() const { return m_name; }

void AIPlayer::dealHoleCards(const Hand& cards) {
    m_hand = cards;
}

Action AIPlayer::getAction(const PlayerView& view) {
    std::string prompt   = PromptBuilder::build(view, m_personalityText);
    std::string response = m_client.sendPrompt(prompt);
    if (response.empty())
        response = m_client.sendPrompt(prompt);  // retry once
    return parseResponse(response);
}

Action AIPlayer::parseResponse(const std::string& response) const {
    std::string upper = response;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);

    if (upper.find("FOLD") != std::string::npos) return Action{Action::Type::Fold};
    if (upper.find("CALL") != std::string::npos) return Action{Action::Type::Call};

    auto pos = upper.find("RAISE");
    if (pos != std::string::npos) {
        try {
            int amount = std::stoi(upper.substr(pos + 6));
            return Action{Action::Type::Raise, amount};
        } catch (...) {
            return fallbackAction();
        }
    }

    return fallbackAction();
}

Action AIPlayer::fallbackAction() const {
    return Action{Action::Type::Call};
}

} // namespace poker
```

---

**Q1.** A reference member makes `AIPlayer` non-copyable and non-move-assignable. References cannot be rebound — once `m_client` is bound in the constructor, it always refers to the same object. The compiler implicitly deletes the copy constructor and copy assignment operator. If you try to copy an `AIPlayer` you get: "use of deleted function AIPlayer::AIPlayer(const AIPlayer&)". Store `AIPlayer` objects via `std::unique_ptr<AIPlayer>` to avoid this.

**Q2.** `upper.substr(pos + 6)` on a six-character string `"RAISE"` (length 5) — `pos + 6` is one past the end — returns an empty string. `std::stoi("")` throws `std::invalid_argument`, which is caught by `catch (...)` and routes to `fallbackAction()`. The `try`/`catch` handles this case explicitly.

</details>

---

### Task 6.4: Wire up CMake

- [ ] Create `src/players/CMakeLists.txt`:

```cmake
add_library(poker_players STATIC
    HumanPlayer.cpp
    AIPlayer.cpp
)

target_include_directories(poker_players PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(poker_players PUBLIC
    poker_core
    poker_ai
)
```

- [ ] Uncomment `add_subdirectory(players)` in `src/CMakeLists.txt`.

- [ ] Reconfigure and build:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
```

<details>
<summary>Concepts</summary>

> **Concept: `poker_ai` is a PUBLIC dependency of `poker_players`**
>
> `AIPlayer.hpp` exposes `ILLMClient&` in its public constructor — consumers of `AIPlayer` need to see `ILLMClient`. Linking `poker_ai` as `PUBLIC` propagates it transitively:
>
> ```
> poker_players (PUBLIC) → poker_ai (PUBLIC) → poker_core
>                                             → nlohmann_json
>                                             → httplib
> ```
>
> Any target that links `poker_players` gets the full chain automatically — no need to list `poker_ai` separately on the executable or test targets.

> **Concept: Library name `poker_players` vs `players`**
>
> Prefixing with `poker_` avoids clashing with CMake built-in targets or other libraries named `players`. `poker_core`, `poker_ai`, `poker_players` form a consistent naming scheme across the project.

**Questions — think through these before checking answers:**
1. `HumanPlayer.cpp` includes `<future>` and `<atomic>`. Do these need to appear in `target_link_libraries`?
2. Why is `poker_ai` listed on `poker_players` rather than on the eventual main executable?

</details>

<details>
<summary>Answers</summary>

**A1.** No. `<future>` and `<atomic>` are part of the C++ standard library, which CMake links automatically when `CMAKE_CXX_STANDARD` is set to 17. Standard library headers do not appear as separate CMake link targets.

**A2.** Because `AIPlayer.cpp` uses `ILLMClient` and `PromptBuilder` — both defined in `poker_ai` — directly in its implementation. The dependency is structural: `AIPlayer.cpp` would fail to compile without the `poker_ai` headers. Putting it only on the executable would mean the compile dependency is implicit and fragile. The rule is: each target declares the dependencies it directly uses.

</details>

---

### Task 6.5: Write `AIPlayer` tests

- [ ] Create `tests/players/MockLLMClient.hpp`:

```cpp
#pragma once
#include "ai/ILLMClient.hpp"
#include <string>

namespace poker {

class MockLLMClient : public ILLMClient {
public:
    std::string responseToReturn = "CALL";

    std::string sendPrompt(const std::string&) override {
        return responseToReturn;
    }
};

} // namespace poker
```

- [ ] Create `tests/players/test_ai_player.cpp`. **Your turn.** Write tests for:
  - Mock returns `"FOLD"` → `getAction()` returns `Action::Type::Fold`
  - Mock returns `"RAISE 200"` → type is `Raise`, amount is `200`
  - Mock returns `"raise 50"` (lowercase) → type is `Raise`, amount is `50`
  - Mock returns garbage → type is `Call` (fallback)

- [ ] Create `tests/players/CMakeLists.txt`:

```cmake
add_executable(players_tests
    test_ai_player.cpp
)

target_include_directories(players_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/tests
)
target_link_libraries(players_tests PRIVATE
    poker_players
    GTest::gtest_main
)

gtest_discover_tests(players_tests)
```

- [ ] Uncomment `add_subdirectory(players)` in `tests/CMakeLists.txt`.

<details>
<summary>Concepts</summary>

> **Concept: Dependency injection pays off in tests**
>
> `AIPlayer` accepts `ILLMClient&` — it never constructs the client itself. In tests, inject `MockLLMClient` to get deterministic, instant responses without starting Ollama:
>
> ```cpp
> MockLLMClient mock;
> mock.responseToReturn = "FOLD";
>
> AIPlayer ai(0, "TestAI", mock, "");
> Action action = ai.getAction(makeTestView());
>
> EXPECT_EQ(action.type, Action::Type::Fold);
> ```
>
> This is the direct payoff for the `ILLMClient` interface from Phase 5: the production path (Ollama) and the test path (mock) are completely interchangeable.

> **Concept: Building a minimal `PlayerView` for tests**
>
> `AIPlayer::getAction()` passes the view to `PromptBuilder::build()`. For tests that only care about the returned action, construct a minimal view with just enough data to avoid crashes:
>
> ```cpp
> static poker::PlayerView makeTestView() {
>     poker::PlayerView v;
>     v.myId   = 0;
>     v.myHand = poker::Hand{
>         poker::Card{poker::Rank::Ace,  poker::Suit::Spades},
>         poker::Card{poker::Rank::King, poker::Suit::Hearts}
>     };
>     v.pot    = 0;
>     v.street = poker::Street::PreFlop;
>     v.chipCounts  = {{0, 200}};
>     v.currentBets = {{0, 0}};
>     v.legal.canCheck = true;
>     return v;
> }
> ```

**Questions — think through these before checking answers:**
1. `MockLLMClient` is in the `poker` namespace. Would it still compile if it were in the global namespace?
2. Why test `"raise 50"` (lowercase) separately from `"RAISE 200"`?

</details>

<details>
<summary>Answers</summary>

**Reference implementation (`test_ai_player.cpp`):**

```cpp
#include <gtest/gtest.h>
#include "players/AIPlayer.hpp"
#include "MockLLMClient.hpp"

using namespace poker;

static PlayerView makeTestView() {
    PlayerView v;
    v.myId   = 0;
    v.myHand = Hand{Card{Rank::Ace, Suit::Spades}, Card{Rank::King, Suit::Hearts}};
    v.pot    = 0;
    v.street = Street::PreFlop;
    v.chipCounts  = {{0, 200}};
    v.currentBets = {{0, 0}};
    v.legal.canCheck = true;
    return v;
}

TEST(AIPlayerTest, FoldResponse) {
    MockLLMClient mock;
    mock.responseToReturn = "FOLD";
    AIPlayer ai(0, "TestAI", mock, "");
    EXPECT_EQ(ai.getAction(makeTestView()).type, Action::Type::Fold);
}

TEST(AIPlayerTest, RaiseResponse) {
    MockLLMClient mock;
    mock.responseToReturn = "RAISE 200";
    AIPlayer ai(0, "TestAI", mock, "");
    Action a = ai.getAction(makeTestView());
    EXPECT_EQ(a.type,   Action::Type::Raise);
    EXPECT_EQ(a.amount, 200);
}

TEST(AIPlayerTest, RaiseLowercase) {
    MockLLMClient mock;
    mock.responseToReturn = "raise 50";
    AIPlayer ai(0, "TestAI", mock, "");
    Action a = ai.getAction(makeTestView());
    EXPECT_EQ(a.type,   Action::Type::Raise);
    EXPECT_EQ(a.amount, 50);
}

TEST(AIPlayerTest, GarbageFallback) {
    MockLLMClient mock;
    mock.responseToReturn = "I have no idea what to do here.";
    AIPlayer ai(0, "TestAI", mock, "");
    EXPECT_EQ(ai.getAction(makeTestView()).type, Action::Type::Call);
}
```

---

**A1.** Yes, it would still compile and link correctly. `AIPlayer`'s constructor takes `ILLMClient&` — a reference to `poker::ILLMClient`. A `MockLLMClient` in the global namespace that inherits from `poker::ILLMClient` is still a valid `poker::ILLMClient`. However, putting it in the `poker` namespace is cleaner: `using namespace poker` at the top of the test file then applies uniformly, and you can write `MockLLMClient` without qualification alongside `AIPlayer`, `PlayerView`, etc.

**A2.** Lowercase input tests the `std::transform` normalisation path in `parseResponse()`. If you only test `"RAISE 200"`, you don't know whether the match succeeded because the response was already uppercase or because normalisation worked. Testing `"raise 50"` confirms the normalisation code is actually executed and that amount extraction still works after the transform.

</details>

---

### Task 6.6: Build and run all tests

- [ ] Build:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
```

- [ ] Run:

```bash
ctest --test-dir build --output-on-failure
```

- [ ] Confirm all test suites pass: `core_tests` (Phase 4), `ai_tests` (Phase 5), and the new `players_tests`.

- [ ] Commit:

```bash
git add src/players/ tests/players/ tests/CMakeLists.txt
git commit -m "feat(players): add IPlayer, HumanPlayer, AIPlayer with tests"
```

---

## Evaluation

Run `/phase-verify` to compile and get automated feedback on your implementation.

### Build checklist

- [ ] `cmake --build` exits with code `0`; `libpoker_players.a` and `players_tests` binary are produced
- [ ] `ctest` output ends with `100% tests passed, 0 tests failed`
- [ ] All three test suites listed: `core_tests`, `ai_tests`, `players_tests`
- [ ] No `undefined reference` linker errors

### Concept checklist

- [ ] Does `HumanPlayer::getAction()` replace `m_promise` and `m_future` at the start of every call?
- [ ] Is the promise replaced *before* `m_waitingForInput` is set to `true`?
- [ ] Is `m_waitingForInput` declared as `std::atomic<bool>`, not plain `bool`?
- [ ] Does `AIPlayer` hold `m_client` as `ILLMClient&` (reference), not by value or pointer?
- [ ] Does `parseResponse()` normalise to uppercase before matching?
- [ ] Does `AIPlayer::getAction()` retry once on empty response before falling back?
- [ ] Does `MockLLMClient` inherit from `poker::ILLMClient` (with the namespace)?
- [ ] Is `add_subdirectory(players)` uncommented in both `src/CMakeLists.txt` and `tests/CMakeLists.txt`?

### Common mistakes

| Mistake | Symptom | Fix |
|---|---|---|
| Not replacing `m_promise` each call | `std::future_error: Promise already satisfied` on second `getAction()` call | Reassign `m_promise = std::promise<Action>{}` and `m_future = m_promise.get_future()` at the top of `getAction()` |
| Setting flag before replacing promise | Rare deadlock: `provideAction()` fires before new promise is ready; `get()` hangs forever | Replace promise → get future → set flag (always this order) |
| `ILLMClient` stored by value | Compile error: cannot instantiate abstract class | Declare as `ILLMClient& m_client` |
| `parseResponse()` uses `==` instead of `find` | `"- FOLD"` or `"fold\n"` not matched; always falls back to Call | Use `find` after normalising to uppercase |
| Forgetting `MockPlayer::getName()` | Compile error: MockPlayer is abstract | Add `std::string getName() const override { return "MockPlayer"; }` |
| Missing `add_subdirectory(players)` in `tests/CMakeLists.txt` | `players_tests` target never defined; `ctest` finds no new tests | Uncomment the line and re-run CMake configure |
