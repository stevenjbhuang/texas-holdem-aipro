# Phase 5 — AI Layer: ILLMClient, OllamaClient, PromptBuilder

**Concepts you'll learn:** Pure abstract interfaces, virtual destructors, dependency injection, HTTP clients with cpp-httplib, JSON serialisation with nlohmann/json, prompt engineering.

**Previous phase:** [Phase 4 — Core Tests](phase-4-core-tests.md)
**Next phase:** [Phase 6 — Players Layer](phase-6-players-layer.md)

**Design reminder:** `src/ai/` must have zero dependencies on `src/ui/`, `src/players/`, or SFML. It only knows about `src/core/` types and the network.

---

### Task 5.1: Create `ILLMClient`

The `ILLMClient` interface is the boundary between the players layer and every possible LLM backend (Ollama, Claude API, a test mock). Nothing outside `src/ai/` ever knows which backend is running.

- [x] Create `src/ai/ILLMClient.hpp`:

```cpp
#pragma once
#include <string>

namespace poker {

class ILLMClient {
public:
    virtual ~ILLMClient() = default;
    virtual std::string sendPrompt(const std::string& prompt) = 0;
};

} // namespace poker
```

<details>
<summary>Concepts</summary>

> **Concept: Pure abstract classes as interfaces**
>
> A class with at least one pure virtual method (`= 0`) cannot be instantiated directly. It exists only to define a contract — a set of methods that every concrete subclass must implement:
>
> ```cpp
> class ILLMClient {
> public:
>     virtual std::string sendPrompt(const std::string& prompt) = 0;
> };
>
> ILLMClient client;       // ❌ compile error: cannot instantiate abstract class
> OllamaClient ollama;     // ✓ concrete subclass — implements sendPrompt
> ```
>
> The `I` prefix is a naming convention for interfaces. It communicates intent: this type is never instantiated, only inherited from.

> **Concept: Why the virtual destructor is not optional**
>
> When you delete an object through a base class pointer, C++ calls the destructor of the *static type* (the pointer type), not the *dynamic type* (the actual object). Without `virtual ~ILLMClient()`, deleting an `OllamaClient` through an `ILLMClient*` calls `ILLMClient`'s destructor and skips `OllamaClient`'s — leaking any resources `OllamaClient` owns:
>
> ```cpp
> ILLMClient* p = new OllamaClient(...);
> delete p;  // ← without virtual dtor: UB — OllamaClient's dtor never called
>            //   with virtual dtor: correct — OllamaClient's dtor runs first
> ```
>
> `= default` tells the compiler to generate the destructor body automatically — you're only marking it virtual.

> **Concept: Dependency injection**
>
> `AIPlayer` (Phase 6) will receive an `ILLMClient*` in its constructor — it never constructs the client itself. This is dependency injection: the object receives its dependencies from the outside rather than creating them internally.
>
> ```cpp
> // Without DI — hard to test, coupled to Ollama:
> class AIPlayer {
>     OllamaClient m_client{"llama3.2", "http://localhost:11434"};
> };
>
> // With DI — testable, backend-agnostic:
> class AIPlayer {
>     AIPlayer(ILLMClient* client) : m_client(client) {}
>     ILLMClient* m_client;
> };
>
> // In tests: inject MockLLMClient
> // In production: inject OllamaClient
> ```
>
> The `ILLMClient` interface is what makes injection possible: the compiler only needs to know the interface to compile `AIPlayer`, not which backend is used.

**Questions — think through these before checking answers:**
1. `ILLMClient` has only one method. Why bother with an interface at all — why not just pass an `OllamaClient` directly?
2. What would happen if `ILLMClient` had `~ILLMClient()` (non-virtual) instead of `virtual ~ILLMClient() = default`? Does the compiler warn you?

</details>

<details>
<summary>Answers</summary>

**Q1.** Two reasons. First, testability: `OllamaClient` makes real HTTP calls. If `AIPlayer` depended on it directly, every test would need a running Ollama server. A `MockLLMClient` returns a canned string instantly — no network. Second, extensibility: adding a `ClaudeClient` or `OpenAIClient` later requires zero changes to `AIPlayer`. The interface absorbs the variation.

**Q2.** The destructor is non-virtual — deletion through `ILLMClient*` is undefined behaviour. The compiler does **not** warn about this by default. Clang and GCC will only warn if you enable `-Wnon-virtual-dtor`, which most projects don't. This is a silent bug. The rule of thumb: every class that is meant to be used polymorphically must have a `virtual` destructor.

</details>

---

### Task 5.2: Create personality config files

Personality files are plain Markdown injected at the top of every prompt. They set tone, strategy, and — critically — the exact output format the AI must follow.

- [x] Create `config/personalities/default.md`:

```markdown
# Default Player

You are a balanced Texas Hold'em poker player. You play solid, fundamental poker.

## Style
- Play tight-aggressive: only enter pots with strong hands, but bet confidently when you do.
- Fold weak hands preflop without hesitation.
- Consider pot odds before calling.

## Decision format
You MUST respond with exactly one of:
- FOLD
- CALL
- RAISE <amount>   (e.g. RAISE 100)

No explanation. No other text. Just the action.
```

- [x] Create `config/personalities/aggressive.md` — write your own. Make this player raise often, bluff occasionally, rarely fold.

- [x] Create `config/personalities/cautious.md` — conservative player, folds marginal hands, rarely bluffs.

<details>
<summary>Concepts</summary>

> **Concept: Output format constraints in prompts**
>
> LLMs are chatty by default. Without an explicit format constraint, a model might respond: *"Given the community cards showing a possible straight draw and your opponent's aggressive betting pattern, I think the safest play here would be to fold."*
>
> `PromptBuilder` will need to parse the response into a concrete `Action`. That requires a predictable format. The "Decision format" section at the bottom of each personality file is the constraint: only one of three patterns is valid output. The parser in `AIPlayer` (Phase 6) will scan for `FOLD`, `CALL`, or `RAISE <n>` and ignore everything else.
>
> Put the format instruction at the **end** of the personality text, immediately before the decision — models follow the most recent instruction most reliably.

> **Concept: Personality as a system prompt**
>
> In chat-style LLM APIs (Claude, GPT-4) there is an explicit "system" role for the model's persona. Ollama's `/api/generate` endpoint uses a single flat prompt string, so the personality text is injected at the top — it functions as an informal system prompt. The game state follows as the "user message".

**Questions — think through these before checking answers:**
1. The `aggressive.md` personality says "raise often". The model reads this instruction once per hand, on every action request. Does that mean it will raise every single action, even when holding 7-2 offsuit on the river with a paired board? What limits its behaviour?
2. Why is the format instruction placed at the *end* of the personality file rather than the beginning?

</details>

<details>
<summary>Answers</summary>

**Q1.** The game state in the prompt provides context that modulates the personality instruction. Even an "aggressive" model can see pot odds, stack sizes, and board texture — and if the model has any poker knowledge, it will temper aggression based on that context. The personality is a bias, not an override. This is also why prompt engineering matters: a well-crafted aggressive persona says *when* and *how much* to raise, not just "raise always".

**Q2.** Autoregressive language models predict each token based on all previous tokens. The most recently seen instruction has the strongest influence on the very next token generated. By placing the format rule immediately before the model produces its response, you maximise the chance it follows the format exactly. Placing it at the top means several hundred tokens of game state intervene before the model generates its answer — the format instruction fades in influence.

</details>

---

### Task 5.3: Implement `PromptBuilder`

`PromptBuilder` serialises a `PlayerView` into a complete prompt string. It takes the `PlayerView` (not `GameState`) because `PlayerView` already filters out opponent cards — there is no risk of accidentally leaking private information.

- [x] Create `src/ai/PromptBuilder.hpp`:

```cpp
#pragma once
#include "core/PlayerView.hpp"
#include <string>

namespace poker {

class PromptBuilder {
public:
    // Builds a complete prompt for the acting player.
    // personalityText: full contents of the personality .md file
    // rulesText:       optional contents of config/rules.md — injected after personality,
    //                  before game state. Leave empty for models that already know poker.
    static std::string build(const PlayerView& view,
                             const std::string& personalityText,
                             const std::string& rulesText = "");

private:
    static std::string streetName(Street s);
    static std::string formatCard(const Card& c);
    static std::string formatHand(const Hand& h);
    static std::string formatCommunity(const std::vector<Card>& cards);
};

} // namespace poker
```

- [x] Implement `src/ai/PromptBuilder.cpp`. Your prompt must include:
  1. The personality text (verbatim, at the top)
  2. Optional rules text — injected after personality, before game state, only when non-empty
  3. The current street name and pot size
  4. The acting player's own hole cards
  5. The community cards (or "none" pre-flop)
  6. Each player's chip count and current street bet — mark the acting player as "you", folded players as folded
  7. The legal actions — read directly from `view.legal`, no arithmetic here

`PromptBuilder` is a pure text formatter. All game logic (what actions are legal, what they cost) is pre-computed in `makePlayerView()` and available on `view.legal`.

<details>
<summary>Concepts</summary>

> **Concept: `static` methods on a utility class**
>
> `PromptBuilder` holds no state — every method only reads its arguments. Making `build` and the helpers `static` expresses this: you call `PromptBuilder::build(view, text)` without constructing an instance. The class is just a namespace with private helpers.
>
> An alternative design is a free function in a `poker` namespace. The class approach is chosen here so the private helpers can't be called from outside without going through `build` — a mild form of encapsulation.

> **Concept: Legal actions belong on `PlayerView`, not in `PromptBuilder`**
>
> `PromptBuilder` is a text formatter — it should only convert data to strings, never compute game rules. Legal action availability (can the player raise? what does a call cost?) is game logic derived from `currentBets`, `chipCounts`, and `minRaise`.
>
> This logic lives in `makePlayerView()` in `src/core/PlayerView.hpp`, which populates a `LegalActions` struct on the view:
>
> ```cpp
> view.legal.canCheck   = (callCost == 0);
> view.legal.canCall    = (callCost > 0);
> view.legal.callCost   = maxBet - myBet;
> view.legal.canRaise   = (minRaiseTo <= maxRaiseTo);
> view.legal.minRaiseTo = maxBet + state.minRaise;
> view.legal.maxRaiseTo = myChips + myBet;  // all-in cap
> ```
>
> `PromptBuilder` reads `view.legal.canRaise`, `view.legal.callCost`, etc. directly — no arithmetic. This also benefits the UI renderer (Phase 7), which reads the same fields to decide which action buttons to show.

> **Concept: Why `PlayerView` instead of `GameState + PlayerId`**
>
> `GameState` contains `holeCards` for *all* players. Passing it to `PromptBuilder` requires discipline: you must never format `state.holeCards[otherId]`. `PlayerView` makes it structurally impossible — it only has `myHand`. The compiler enforces the invariant, not the developer's attention.
>
> This is the "make illegal states unrepresentable" principle: prefer types that can only hold valid values over types that can hold invalid values but require careful handling.

> **Concept: String building with `std::ostringstream`**
>
> For building multi-line strings, `std::ostringstream` is ergonomic and avoids repeated `std::string` concatenation (which allocates on every `+`):
>
> ```cpp
> #include <sstream>
>
> std::ostringstream ss;
> ss << "Street: " << streetName(view.street) << "\n";
> ss << "Pot: $"   << view.pot                << "\n";
> return ss.str();
> ```

**Questions — think through these before checking answers:**
1. The `minRaise` field in `PlayerView` stores the minimum *raise amount*, not the minimum total bet. If `maxBet` is 10 and `minRaise` is 10, what is the minimum legal total bet a player can make when raising?
2. Why include each player's current street bet in the prompt, not just their chip count?

</details>

<details>
<summary>Answers</summary>

**Reference implementation (`PromptBuilder.cpp`):**

```cpp
#include "ai/PromptBuilder.hpp"
#include <sstream>
#include <algorithm>

namespace poker {

std::string PromptBuilder::streetName(Street s) {
    switch (s) {
        case Street::PreFlop:  return "Pre-Flop";
        case Street::Flop:     return "Flop";
        case Street::Turn:     return "Turn";
        case Street::River:    return "River";
        case Street::Showdown: return "Showdown";
    }
    return "Unknown";
}

std::string PromptBuilder::formatCard(const Card& c) {
    return c.toShortString();
}

std::string PromptBuilder::formatHand(const Hand& h) {
    return formatCard(h.first) + " " + formatCard(h.second);
}

std::string PromptBuilder::formatCommunity(const std::vector<Card>& cards) {
    if (cards.empty()) return "none";
    std::string result;
    for (size_t i = 0; i < cards.size(); ++i) {
        if (i > 0) result += " ";
        result += formatCard(cards[i]);
    }
    return result;
}

std::string PromptBuilder::build(const PlayerView& view,
                                 const std::string& personalityText,
                                 const std::string& rulesText) {
    std::ostringstream ss;

    // 1. Personality
    ss << personalityText << "\n\n---\n\n";

    // 2. Optional rules context (for small/custom models that may not know poker)
    if (!rulesText.empty())
        ss << rulesText << "\n\n---\n\n";

    // 3. Game state
    ss << "## Game State\n";
    ss << "Street: " << streetName(view.street) << "\n";
    ss << "Pot: $"   << view.pot                << "\n\n";

    // 4. Your hand
    ss << "## Your Hand\n";
    ss << formatHand(view.myHand) << "\n\n";

    // 5. Community cards
    ss << "## Community Cards\n";
    ss << formatCommunity(view.communityCards) << "\n\n";

    // 6. Players
    ss << "## Players\n";
    for (auto& [id, chips] : view.chipCounts) {
        int bet = view.currentBets.count(id) ? view.currentBets.at(id) : 0;
        ss << "- Player " << id;
        if (id == view.myId)          ss << " (you)";
        if (id == view.dealerButton)  ss << " [dealer]";
        if (id == view.smallBlindSeat && id != view.dealerButton) ss << " [SB]";
        if (id == view.bigBlindSeat)  ss << " [BB]";
        if (view.foldedPlayers.count(id)) {
            ss << ": folded\n";
        } else {
            ss << ": $" << chips << " | bet: $" << bet << "\n";
        }
    }
    ss << "\n";

    // 7. Legal actions — read from view.legal, no arithmetic here
    ss << "## Legal Actions\n";
    ss << "- FOLD\n";
    if (view.legal.canCheck)
        ss << "- CHECK\n";
    if (view.legal.canCall)
        ss << "- CALL $" << view.legal.callCost << "\n";
    if (view.legal.canRaise)
        ss << "- RAISE <amount>  (min: $" << view.legal.minRaiseTo
           << ", max: $" << view.legal.maxRaiseTo << ")\n";

    ss << "\n---\n\n";
    ss << "Respond with exactly one action from Legal Actions above. No explanation.";

    return ss.str();
}

} // namespace poker
```

---

**Q1.** The minimum total bet is `maxBet + minRaise = 10 + 10 = 20`. `minRaise` is the minimum *increase* above the current highest bet, not the total. This is standard poker: the minimum re-raise must be at least the size of the previous raise.

**Q2.** The current street bet tells the model how much it has already committed this street. Without it, the model can't calculate pot odds or know how much a call actually costs. Example: if the pot is $20, the model has bet $5 this street, and the current max bet is $10, the model needs to see both numbers to know it only owes $5 more (not $10) to call.

</details>

---

### Task 5.4: Implement `OllamaClient`

`OllamaClient` sends the prompt to a local Ollama server and returns the raw text response. It has no poker knowledge — it just does HTTP.

Ollama's generate endpoint:
```
POST /api/generate
Body:     { "model": "llama3.2", "prompt": "...", "stream": false }
Response: { "response": "CALL", "done": true, ... }
```

- [ ] Create `src/ai/OllamaClient.hpp`:

```cpp
#pragma once
#include "ILLMClient.hpp"
#include <string>

namespace poker {

class OllamaClient : public ILLMClient {
public:
    OllamaClient(std::string model, std::string endpoint);

    std::string sendPrompt(const std::string& prompt) override;

private:
    std::string m_model;
    std::string m_endpoint;  // e.g. "http://localhost:11434"
};

} // namespace poker
```

- [ ] Implement `src/ai/OllamaClient.cpp`. Hints:
  - `#include <httplib.h>` for cpp-httplib — `httplib::Client` takes `(host, port)` separately, not a full URL
  - `#include <nlohmann/json.hpp>` for JSON — use `nlohmann::json::parse(body, nullptr, /*exceptions=*/false)` to avoid exceptions on malformed responses
  - Return `""` on any error — the caller (`AIPlayer`) will handle the fallback

<details>
<summary>Concepts</summary>

> **Concept: cpp-httplib basics**
>
> cpp-httplib is a single-header synchronous HTTP library. `Client` takes host and port separately — you need to parse your endpoint URL first:
>
> ```cpp
> // ❌ Wrong — httplib::Client does not accept a full URL
> httplib::Client client("http://localhost:11434");
>
> // ✓ Correct — host and port separate
> httplib::Client client("localhost", 11434);
>
> auto res = client.Post("/api/generate", body, "application/json");
> if (!res || res->status != 200) return "";  // error: no connection or non-200
> std::string text = res->body;               // raw JSON response body
> ```
>
> Set timeouts so the game doesn't hang if Ollama is slow:
> ```cpp
> client.set_connection_timeout(5);   // seconds
> client.set_read_timeout(30);        // seconds — LLMs can be slow
> ```

> **Concept: nlohmann/json**
>
> nlohmann/json is a modern header-only JSON library. The key methods you need:
>
> ```cpp
> #include <nlohmann/json.hpp>
> using json = nlohmann::json;
>
> // Serialise a C++ object to JSON string
> json body;
> body["model"]  = "llama3.2";
> body["prompt"] = "...";
> body["stream"] = false;
> std::string bodyStr = body.dump();   // → {"model":"llama3.2","prompt":"...","stream":false}
>
> // Parse a JSON string — use the no-exception overload
> auto parsed = json::parse(responseStr, nullptr, /*allow_exceptions=*/false);
> if (parsed.is_discarded()) return "";  // malformed JSON
>
> // Extract a field with a default value if missing
> std::string response = parsed.value("response", "");
> ```

> **Concept: Parsing the endpoint URL**
>
> `m_endpoint` is stored as `"http://localhost:11434"`. You need to extract `"localhost"` and `11434` for `httplib::Client`. A minimal parser:
>
> ```cpp
> std::string host = m_endpoint;
> int port = 80;
>
> // Strip "http://" or "https://"
> for (auto prefix : {"https://", "http://"}) {
>     std::string p = prefix;
>     if (host.substr(0, p.size()) == p)
>         host = host.substr(p.size());
> }
>
> // Split on last ":"
> auto colon = host.rfind(':');
> if (colon != std::string::npos) {
>     port = std::stoi(host.substr(colon + 1));
>     host = host.substr(0, colon);
> }
> ```
>
> `rfind` (reverse find) is used instead of `find` to avoid splitting on the `:` in `https://`.

> **Concept: Returning empty string on error**
>
> `OllamaClient::sendPrompt` never throws. On any failure (connection refused, timeout, malformed JSON, missing `"response"` field), it returns `""`. The caller — `AIPlayer::getAction` in Phase 6 — detects the empty string and falls back to a safe default action (e.g. `Fold`).
>
> This is the "fail-safe default" pattern: the AI layer degrades gracefully rather than crashing the game when Ollama is unreachable.

**Questions — think through these before checking answers:**
1. `stream: false` is included in the request body. What would happen if you omitted it and Ollama defaulted to streaming mode?
2. `json::parse` is called with `allow_exceptions=false`. What is the alternative, and why is the no-exception version preferred here?

</details>

<details>
<summary>Answers</summary>

**Reference implementation (`OllamaClient.cpp`):**

```cpp
#include "ai/OllamaClient.hpp"
#include <httplib.h>
#include <nlohmann/json.hpp>

namespace poker {

OllamaClient::OllamaClient(std::string model, std::string endpoint)
    : m_model(std::move(model)), m_endpoint(std::move(endpoint)) {}

std::string OllamaClient::sendPrompt(const std::string& prompt) {
    // Parse endpoint → host + port
    std::string host = m_endpoint;
    int port = 80;
    for (auto prefix : {"https://", "http://"}) {
        std::string p = prefix;
        if (host.size() >= p.size() && host.substr(0, p.size()) == p)
            host = host.substr(p.size());
    }
    auto colon = host.rfind(':');
    if (colon != std::string::npos) {
        port = std::stoi(host.substr(colon + 1));
        host = host.substr(0, colon);
    }

    httplib::Client client(host, port);
    client.set_connection_timeout(5);
    client.set_read_timeout(30);

    nlohmann::json body;
    body["model"]  = m_model;
    body["prompt"] = prompt;
    body["stream"] = false;

    auto res = client.Post("/api/generate", body.dump(), "application/json");
    if (!res || res->status != 200) return "";

    auto parsed = nlohmann::json::parse(res->body, nullptr, /*exceptions=*/false);
    if (parsed.is_discarded()) return "";
    return parsed.value("response", "");
}

} // namespace poker
```

---

**Q1.** In streaming mode Ollama sends a sequence of newline-delimited JSON objects, one per token, as the model generates them. `httplib` would receive the first chunk and return it as `res->body` — a single partial JSON object like `{"response":"C","done":false}`. Parsing `"response"` from that gives `"C"` — not a complete action. The response would always parse as garbage. `stream: false` tells Ollama to wait until generation is complete and return one JSON object.

**Q2.** The alternative is letting `json::parse` throw `json::parse_error` on malformed input. The no-exception version (`allow_exceptions=false`) returns a discarded object instead, which you check with `is_discarded()`. The no-exception version is preferred here because `sendPrompt` is declared not to throw — callers rely on it returning a string or `""`. Letting an exception escape would violate the contract and crash the game loop if Ollama sends a malformed response (which can happen under load or after a model error).

</details>

---

### Task 5.5: Wire up CMake

- [ ] Create `src/ai/CMakeLists.txt`:

```cmake
add_library(poker_ai STATIC
    OllamaClient.cpp
    PromptBuilder.cpp
)

target_include_directories(poker_ai PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(poker_ai PUBLIC
    poker_core
    nlohmann_json::nlohmann_json
    httplib::httplib
)
```

- [ ] Activate the layer in `src/CMakeLists.txt` — uncomment:

```cmake
add_subdirectory(ai)
```

- [ ] Reconfigure and build:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
```

<details>
<summary>Concepts</summary>

> **Concept: CMake target names for FetchContent dependencies**
>
> FetchContent sets up the dependency exactly as if you had `add_subdirectory`-ed it. The CMake target name is whatever the dependency's own `CMakeLists.txt` exports. You need to check the dependency's own CMakeLists rather than guessing:
>
> | Dependency | FetchContent name | Linkable target |
> |---|---|---|
> | nlohmann_json | `nlohmann_json` | `nlohmann_json::nlohmann_json` |
> | cpp-httplib | `httplib` | `httplib::httplib` |
> | yaml-cpp | `yaml-cpp` | `yaml-cpp` (no namespace — non-standard) |
> | googletest | `googletest` | `GTest::gtest_main` |
>
> Note that yaml-cpp uses the old CMake convention (no `::` namespace). If you accidentally write `yaml-cpp::yaml-cpp`, CMake will error: *target not found*.

> **Concept: Why yaml-cpp is not linked into `poker_ai`**
>
> `OllamaClient` and `PromptBuilder` have no reason to parse YAML. Config loading (model name, endpoint URL) happens in `main.cpp`. Keeping yaml-cpp out of `poker_ai` means: the AI layer has no config-format dependency, and any test of the AI layer does not need to provide YAML files.
>
> Adding a dependency "just in case" or "because it's convenient" is how libraries end up with bloated dependency graphs. Add a dependency to a target only when that target's source files `#include` a header from it.

> **Concept: `PUBLIC` vs `PRIVATE` on `target_link_libraries`**
>
> When `poker_ai` links `nlohmann_json::nlohmann_json` as `PUBLIC`, any target that links `poker_ai` also gets `nlohmann_json` transitively. For `PRIVATE`, the dependency is consumed internally and not propagated.
>
> ```cmake
> target_link_libraries(poker_ai PUBLIC  nlohmann_json::nlohmann_json)
> # → AIPlayer (Phase 6) also gets nlohmann_json without listing it explicitly
>
> target_link_libraries(poker_ai PRIVATE nlohmann_json::nlohmann_json)
> # → AIPlayer must link nlohmann_json itself if it needs it
> ```
>
> The practical question: does the *header* of `poker_ai` expose nlohmann/json types to consumers? `ILLMClient.hpp` and `OllamaClient.hpp` do not include any nlohmann headers — so `PRIVATE` would be correct for stricter dependency hygiene. `PUBLIC` is simpler and works fine for this project.

**Questions — think through these before checking answers:**
1. The library target is named `poker_ai`, not `ai`. Why is that preferable?
2. `httplib::httplib` is listed under `PUBLIC` dependencies. Does `AIPlayer` in Phase 6 actually need direct access to httplib headers? Should it be `PRIVATE`?

</details>

<details>
<summary>Answers</summary>

**Q1.** `ai` is a generic name that could clash with another target in a larger project or a CMake module. `poker_ai` is scoped to this project, follows the naming convention already established by `poker_core` and `poker_hand_evaluator`, and is unambiguous in CMake's global target namespace.

**Q2.** `AIPlayer` does not `#include <httplib.h>` — it only calls `ILLMClient::sendPrompt`. So httplib is an *implementation detail* of `OllamaClient`, not part of the public interface of `poker_ai`. Strictly speaking it should be `PRIVATE`. However, since `OllamaClient.hpp` does not include httplib headers either, there's no compile-time leakage either way. `PRIVATE` is the more correct choice; `PUBLIC` is harmless for this project.

</details>

---

### Task 5.6: Write `PromptBuilder` tests and create `MockLLMClient`

`OllamaClient` requires a live Ollama server — not suitable for unit tests. `PromptBuilder` is pure string logic and is straightforward to test. This task also creates `MockLLMClient`, which will be needed by `AIPlayer` tests in Phase 6.

- [ ] Create `tests/ai/MockLLMClient.hpp`:

```cpp
#pragma once
#include "ai/ILLMClient.hpp"
#include <string>

class MockLLMClient : public poker::ILLMClient {
public:
    explicit MockLLMClient(std::string response = "CALL")
        : m_response(std::move(response)) {}

    std::string sendPrompt(const std::string& prompt) override {
        m_lastPrompt = prompt;
        return m_response;
    }

    void setResponse(const std::string& r) { m_response = r; }
    const std::string& lastPrompt() const  { return m_lastPrompt; }

private:
    std::string m_response;
    std::string m_lastPrompt;
};
```

- [ ] Create `tests/ai/test_prompt_builder.cpp`. Write tests for:
  - The personality text appears verbatim in the output
  - The acting player's hole cards appear in the output
  - No opponent's hole cards appear anywhere in the output
  - The community cards (or "none") appear in the output
  - `FOLD` and `CALL` appear in the legal actions section
  - `RAISE` does not appear when the player cannot afford the minimum raise

- [ ] Create `tests/ai/CMakeLists.txt`:

```cmake
add_executable(ai_tests
    test_prompt_builder.cpp
)

target_link_libraries(ai_tests PRIVATE
    poker_ai
    GTest::gtest_main
)

gtest_discover_tests(ai_tests)
```

- [ ] Add `add_subdirectory(ai)` to `tests/CMakeLists.txt`.

<details>
<summary>Concepts</summary>

> **Concept: Testing string output with `EXPECT_NE(str.find(...), std::string::npos)`**
>
> `PromptBuilder::build` returns a multi-line string. The most practical way to assert content is present is `std::string::find`:
>
> ```cpp
> std::string prompt = PromptBuilder::build(view, "my personality");
>
> // Assert "my personality" appears somewhere in the prompt
> EXPECT_NE(prompt.find("my personality"), std::string::npos);
>
> // Assert opponent's card does NOT appear
> EXPECT_EQ(prompt.find("Kd"), std::string::npos);  // opponent has King of Diamonds
> ```
>
> `std::string::npos` is the sentinel value returned when `find` finds no match. `NE(pos, npos)` = "found it". `EQ(pos, npos)` = "not found".

> **Concept: Building a `PlayerView` directly in tests**
>
> `PlayerView` is a plain struct with public fields — you can construct one directly without going through `GameEngine`:
>
> ```cpp
> PlayerView view;
> view.myId      = 0;
> view.myHand    = Hand{Card{Rank::Ace, Suit::Spades}, Card{Rank::King, Suit::Hearts}};
> view.pot       = 20;
> view.street    = Street::Flop;
> view.communityCards = { Card{Rank::Jack, Suit::Spades},
>                         Card{Rank::Ten,  Suit::Hearts},
>                         Card{Rank::Nine, Suit::Diamonds} };
> view.chipCounts   = {{0, 180}, {1, 200}};
> view.currentBets  = {{0, 10},  {1, 10}};
> view.minRaise     = 10;
> ```
>
> This avoids the full GameEngine setup and makes the test fast and readable.

> **Concept: `MockLLMClient::lastPrompt()`**
>
> The `MockLLMClient` stores the last prompt it received. This lets Phase 6 tests verify not just *what action* the AI took, but *what information it was given*:
>
> ```cpp
> MockLLMClient mock("FOLD");
> AIPlayer ai(0, &mock, "personality text");
> ai.getAction(view);
>
> // Verify the prompt contained the right information
> EXPECT_NE(mock.lastPrompt().find("Pot: $20"), std::string::npos);
> ```

**Questions — think through these before checking answers:**
1. The "no opponent cards" test needs to assert that a specific card string is absent. How do you pick which card to look for? What if the acting player's own hand happens to contain that card?
2. `MockLLMClient` stores `m_lastPrompt` as a `std::string`. If `sendPrompt` is called multiple times, it only keeps the last. Is that a problem for the tests you're writing?

</details>

<details>
<summary>Answers</summary>

**Reference implementation (`test_prompt_builder.cpp`):**

```cpp
#include <gtest/gtest.h>
#include "ai/PromptBuilder.hpp"
#include "core/PlayerView.hpp"

using namespace poker;

// Helper: build a minimal two-player view for player 0
static PlayerView makeView() {
    PlayerView v;
    v.myId   = 0;
    v.myHand = Hand{Card{Rank::Ace, Suit::Spades}, Card{Rank::King, Suit::Hearts}};
    // Opponent (player 1) holds Two of Clubs, Three of Diamonds — never appears in view
    v.pot    = 20;
    v.street = Street::Flop;
    v.communityCards = {
        Card{Rank::Jack, Suit::Spades},
        Card{Rank::Ten,  Suit::Hearts},
        Card{Rank::Nine, Suit::Diamonds},
    };
    v.chipCounts  = {{0, 180}, {1, 200}};
    v.currentBets = {{0, 10},  {1, 10}};
    v.minRaise    = 10;
    v.dealerButton   = 0;
    v.smallBlindSeat = 0;
    v.bigBlindSeat   = 1;
    return v;
}

TEST(PromptBuilderTest, ContainsPersonalityText) {
    auto prompt = PromptBuilder::build(makeView(), "BE AGGRESSIVE");
    EXPECT_NE(prompt.find("BE AGGRESSIVE"), std::string::npos);
}

TEST(PromptBuilderTest, ContainsOwnHoleCards) {
    auto prompt = PromptBuilder::build(makeView(), "");
    EXPECT_NE(prompt.find("As"), std::string::npos);  // Ace of Spades
    EXPECT_NE(prompt.find("Kh"), std::string::npos);  // King of Hearts
}

TEST(PromptBuilderTest, ContainsCommunityCards) {
    auto prompt = PromptBuilder::build(makeView(), "");
    EXPECT_NE(prompt.find("Js"), std::string::npos);  // Jack of Spades
    EXPECT_NE(prompt.find("Th"), std::string::npos);  // Ten of Hearts
    EXPECT_NE(prompt.find("9d"), std::string::npos);  // Nine of Diamonds
}

TEST(PromptBuilderTest, PreFlopShowsNoneCommunity) {
    auto v = makeView();
    v.street = Street::PreFlop;
    v.communityCards.clear();
    auto prompt = PromptBuilder::build(v, "");
    EXPECT_NE(prompt.find("none"), std::string::npos);
}

TEST(PromptBuilderTest, ContainsFoldAndCall) {
    auto prompt = PromptBuilder::build(makeView(), "");
    EXPECT_NE(prompt.find("FOLD"), std::string::npos);
    EXPECT_NE(prompt.find("CALL"), std::string::npos);
}

TEST(PromptBuilderTest, RaiseAbsentWhenCannotAfford) {
    auto v = makeView();
    v.chipCounts[0]  = 5;   // player 0 only has $5 left
    v.currentBets[0] = 10;
    v.currentBets[1] = 10;
    v.minRaise = 100;       // minimum raise would cost $100 — impossible
    auto prompt = PromptBuilder::build(v, "");
    EXPECT_EQ(prompt.find("RAISE"), std::string::npos);
}
```

---

**Q1.** Pick a card that the *acting player* definitely does not hold. In `makeView()` the acting player holds Ace of Spades and King of Hearts. The "opponent's hand" is fictional (the `PlayerView` doesn't store it). To test that no opponent card is leaked, you'd instead verify that a card the opponent *would* hold in a real GameState — one different from the acting player's hand — is absent. In practice: construct a `GameState` with known hole cards, call `makePlayerView`, then build the prompt and assert the opponent's short strings don't appear.

**Q2.** For the tests written here, no — each test calls `getAction` once. If you later write a test that calls `getAction` multiple times on the same `AIPlayer` (e.g. testing action sequences across streets), you'd want either a `prompts()` vector or a `callCount()` accessor on `MockLLMClient`. For Phase 6 tests, the single `lastPrompt()` is sufficient.

</details>

---

### Task 5.7: Build and run all tests

- [ ] Build:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
```

- [ ] Run:

```bash
ctest --test-dir build --output-on-failure
```

- [ ] Confirm both test binaries appear and pass: `core_tests` (from Phase 4) and `ai_tests`.

- [ ] Commit:

```bash
git add src/ai/ tests/ai/ tests/CMakeLists.txt config/personalities/
git commit -m "feat(ai): add ILLMClient, OllamaClient, PromptBuilder, MockLLMClient, tests"
```

---

### Task 5.8: Give the AI agent memory

The LLM is stateless — every `sendPrompt` call starts fresh with no knowledge of previous calls. Memory means injecting relevant history *into the prompt*. This task adds a `SessionMemory` class that tracks observable opponent behaviour across hands and injects a short "Opponent Notes" section into every prompt.

There are two scopes:
- **Session memory** — hand-to-hand observations within one run of the game
- **Persistent memory** — saved to disk and reloaded next session

Implement session memory first; persistence is an extension at the end.

#### Step 1: Add `onHandEnd` to `IPlayer`

`AIPlayer` needs to know when a hand ends so it can record showdown information. Add a default no-op method to `IPlayer` so existing implementations don't break:

- [ ] Add to `src/players/IPlayer.hpp`:

```cpp
// Called by GameEngine after each hand completes.
// finalState: the state at the moment the pot was awarded — holeCards contains
//             all non-folded players' hands (showdown hands).
// winner: the PlayerId who was awarded the main pot.
virtual void onHandEnd(const GameState& finalState, PlayerId winner) {}
```

- [ ] Call it from `GameEngine::determineWinner()` after awarding the pot, before returning:

```cpp
// After all pots are awarded and m_startNewHand = true:
for (auto& player : m_players)
    player->onHandEnd(m_state, lastWinner);
```

You'll need to track `lastWinner` inside `determineWinner()` — it's the `PlayerId` who received the final side pot (or the sole non-folded player in the early-exit case).

<details>
<summary>Concepts</summary>

> **Concept: Default method implementations in interfaces**
>
> A pure virtual method (`= 0`) forces every subclass to implement it. A virtual method *with a default body* is optional — subclasses may override it but don't have to. This is the right choice for lifecycle hooks that only some players care about:
>
> ```cpp
> virtual void onHandEnd(const GameState&, PlayerId) {}  // default: do nothing
> ```
>
> `HumanPlayer`, `MockPlayer`, and `NetworkPlayer` all inherit the no-op. Only `AIPlayer` overrides it to update memory. The interface stays backward-compatible — existing implementations need no changes.

> **Concept: Where to call `onHandEnd`**
>
> `determineWinner()` is the single place in `GameEngine` where every hand terminates, whether by showdown or by all-but-one folding. Calling `onHandEnd` there guarantees it fires exactly once per hand regardless of which path terminated it.
>
> Order matters: award chips first, then call `onHandEnd`. This way `finalState.chipCounts` already reflects the outcome — useful if a player wants to observe who gained chips.

**Questions — think through these before checking answers:**
1. `onHandEnd` receives a `GameState` with `holeCards` for all players. In a hand that ended because everyone folded to one player (no showdown), should the AI be able to see the folded players' hole cards?
2. `onHandEnd` is called from the `GameEngine` worker thread. `AIPlayer::onHandEnd` will write to `SessionMemory`. Is there a thread-safety concern?

</details>

<details>
<summary>Answers</summary>

**Q1.** No — folded players' hole cards should not be visible. In real poker, a player who folds face-down never has to reveal their hand. The `GameState::holeCards` map contains all hole cards internally (the engine needs them for evaluation), but `onHandEnd` should only reveal hands of players who were not folded: check `finalState.foldedPlayers.count(id) == 0` before recording a hand. Alternatively, pass a separate `map<PlayerId, Hand> revealedHands` that is pre-filtered by `GameEngine` before calling `onHandEnd`.

**Q2.** `onHandEnd` is called from the `GameEngine` worker thread. `AIPlayer::getAction` is *also* called from the same worker thread (sequentially). Since `GameEngine` calls these methods one at a time — never concurrently — `SessionMemory` is only ever accessed from the worker thread. No mutex is needed unless you add reads from another thread (e.g. a renderer showing opponent stats).

</details>

---

#### Step 2: Implement `PlayerStats` and `SessionMemory`

- [ ] Create `src/ai/SessionMemory.hpp`:

```cpp
#pragma once
#include "core/GameState.hpp"
#include "core/Types.hpp"
#include <map>
#include <string>
#include <vector>

namespace poker {

struct PlayerStats {
    int handsObserved    = 0;
    int voluntaryEntered = 0;  // VPIP: limped or raised preflop
    int preflopRaises    = 0;  // PFR: raised preflop
    int totalRaises      = 0;  // raises across all streets
    int totalFolds       = 0;
    int foldedToRaise    = 0;  // folded when the last action was a raise
    std::vector<std::string> showdownNotes;  // e.g. "Won with AA", "Bluffed with 72o"
};

class SessionMemory {
public:
    // Record an observed action. facingRaise = true when the acting player
    // is responding to a raise (used to compute fold-to-aggression stat).
    void recordAction(PlayerId actor, Action action,
                      Street street, bool facingRaise);

    // Record a showdown hand. Called from AIPlayer::onHandEnd for each
    // non-folded player whose hand was revealed.
    void recordShowdown(PlayerId id, const Hand& hand, bool won);

    // Increment handsObserved for all tracked players. Call once per hand end.
    void recordHandEnd();

    // Returns a formatted "Opponent Notes" string for prompt injection.
    // Excludes ownId (you don't narrate yourself) and players with < minHands observed.
    std::string formatOpponentNotes(PlayerId ownId, int minHands = 2) const;

    bool hasData(PlayerId id) const;
    const PlayerStats& statsFor(PlayerId id) const;

    // Persistence
    void        save(const std::string& path) const;  // writes JSON
    static SessionMemory load(const std::string& path);

private:
    std::map<PlayerId, PlayerStats> m_stats;
};

} // namespace poker
```

- [ ] Implement `src/ai/SessionMemory.cpp`. Key logic:

  - `recordAction`: increment `voluntaryEntered` if PreFlop and action is not Fold; increment `preflopRaises` / `totalRaises` / `totalFolds` / `foldedToRaise` as appropriate.
  - `formatOpponentNotes`: for each tracked opponent (excluding `ownId`), generate one line. Include VPIP%, aggression label (passive/balanced/aggressive based on raise frequency), and any showdown notes.
  - `save` / `load`: use `nlohmann::json` — `SessionMemory` already lives in `src/ai/` which links it.

Example formatted output (injected into the prompt between Community Cards and Players):

```
## Opponent Notes
- Player 1 (8 hands): Enters pot often (75%). Raised 5 times total — aggressive.
  Showdown: Won with Ah Kd, Bluffed with 7c 2h.
- Player 2 (8 hands): Tight — enters pot 25% of time. Rarely raises. Never caught bluffing.
```

<details>
<summary>Concepts</summary>

> **Concept: VPIP and PFR — the two most important poker stats**
>
> **VPIP** (Voluntarily Put money In Pot): the percentage of hands where a player chose to call or raise preflop (not fold, not post a blind). A player with VPIP > 40% plays too many hands (loose). < 20% is tight.
>
> **PFR** (Pre-Flop Raise): the percentage of hands where they *raised* preflop. PFR / VPIP ratio reveals aggression: a player with VPIP 30% and PFR 25% is aggressive (raises most hands they play). VPIP 30% PFR 5% is passive (calls a lot, rarely raises).
>
> These two numbers give the model a compact, meaningful read on each opponent with just a few hands of data.

> **Concept: Minimum sample size (`minHands`)**
>
> After 1-2 hands, stats are noise. A player who raised once in two hands could be a maniac or just got dealt AA. `minHands = 2` (the default) is low — in a real HUD you'd want 20+. For this game, 2 is enough to suppress empty notes for opponents you've barely seen, while still providing something useful early.

> **Concept: Qualitative labels over raw numbers**
>
> The prompt says "Raised 5 times — aggressive" rather than "PFR: 62.5%". LLMs process natural language descriptions more reliably than raw statistics. Convert numeric thresholds to labels at the `formatOpponentNotes` boundary:
>
> ```cpp
> // Raise rate thresholds → label
> float raiseRate = totalRaises / (float)handsObserved;
> std::string label = raiseRate > 0.4f ? "aggressive"
>                   : raiseRate > 0.2f ? "balanced"
>                   : "passive";
> ```

**Questions — think through these before checking answers:**
1. `recordAction` is called by `AIPlayer` for actions it *observes* in the `PlayerView`. But `AIPlayer` only calls `getAction` for its own turn. How does it observe *other* players' actions between its own turns?
2. Showdown notes accumulate indefinitely. After 100 hands, a player might have 40 showdown entries in their `PlayerStats`. What is the problem with injecting all of them into the prompt?

</details>

<details>
<summary>Answers</summary>

**Q1.** This is a genuine limitation of the current architecture. `AIPlayer::getAction` is only called for the AI's own turn, so it can only observe the game state *at the moment it acts* — not the individual actions that happened between its turns. The `PlayerView` shows each player's current bet and fold status, so the AI can infer: "Player 2 went from $0 to $40 since my last turn — they raised". It's indirect but workable. A richer approach (Phase extension): add an `onActionObserved(PlayerId, Action)` callback to `IPlayer` that `GameEngine` calls after every action, giving the AI a complete action-by-action feed.

**Q2.** Token budget and relevance. Most LLM context windows are large enough to hold 40 notes, but the signal-to-noise ratio drops as old notes become stale. More practically: injecting 40 lines of showdown history for one player dwarfs the actual game state, which is what the model should be focused on. Fix: keep only the most recent N showdown notes (e.g. `maxShowdownNotes = 5`), dropping older ones as new ones arrive.

</details>

---

#### Step 3: Update `PromptBuilder` and `AIPlayer`

- [ ] Add an optional `memory` parameter to `PromptBuilder::build`:

```cpp
// PromptBuilder.hpp — final signature
static std::string build(const PlayerView& view,
                         const std::string& personalityText,
                         const std::string& rulesText = "",
                         const SessionMemory* memory = nullptr);
```

- [ ] In `PromptBuilder.cpp`, inject the notes section between Community Cards and Players when `memory != nullptr` and there is data for at least one opponent:

```cpp
if (memory) {
    std::string notes = memory->formatOpponentNotes(view.myId);
    if (!notes.empty())
        ss << "## Opponent Notes\n" << notes << "\n";
}
```

- [ ] Update `AIPlayer` (Phase 6 adds `AIPlayer`, but sketch the interface now):

```cpp
// AIPlayer owns the memory
class AIPlayer : public IPlayer {
    SessionMemory m_memory;

    Action getAction(const PlayerView& view) override {
        // observe what we can from the view
        updateMemoryFromView(view);
        std::string prompt = PromptBuilder::build(view, m_personality, &m_memory);
        return parseAction(m_client->sendPrompt(prompt));
    }

    void onHandEnd(const GameState& finalState, PlayerId winner) override {
        // record revealed showdown hands
        for (auto& [id, hand] : finalState.holeCards)
            if (!finalState.foldedPlayers.count(id))
                m_memory.recordShowdown(id, hand, id == winner);
        m_memory.recordHandEnd();
        m_memory.save("memory/profiles.json");
    }
};
```

`updateMemoryFromView` infers opponent actions from the `PlayerView`: compare bet sizes and fold status to what was last seen to reconstruct what happened between turns.

<details>
<summary>Concepts</summary>

> **Concept: Inferring actions from state snapshots**
>
> `PlayerView` is a snapshot, not a stream of events. To infer what Player 2 did since your last turn:
>
> ```cpp
> void AIPlayer::updateMemoryFromView(const PlayerView& view) {
>     for (auto& [id, bet] : view.currentBets) {
>         if (id == view.myId) continue;
>         bool nowFolded   = view.foldedPlayers.count(id);
>         int  prevBet     = m_lastKnownBets.count(id) ? m_lastKnownBets[id] : 0;
>
>         if (nowFolded && !m_lastKnownFolded.count(id))
>             m_memory.recordAction(id, Action{Action::Type::Fold}, view.street, prevBet < maxBet);
>         else if (bet > prevBet)
>             m_memory.recordAction(id, Action{Action::Type::Raise}, view.street, false);
>
>         m_lastKnownBets[id] = bet;
>     }
>     m_lastKnownFolded = view.foldedPlayers;
> }
> ```
>
> This gives approximate readings — you can detect raises and folds but not the exact raise amount. Good enough for qualitative opponent profiling.

> **Concept: Memory as a file on disk**
>
> Saving to `memory/profiles.json` after every hand creates a persistent opponent model that survives game restarts. Add `memory/` to `.gitignore` — it's runtime data, not source. Over many sessions against the same players (identified by name or a stable ID), the AI accumulates a genuine multi-session read on each opponent.

**Questions — think through these before checking answers:**
1. `m_memory.save()` is called inside `onHandEnd()`, which runs on the `GameEngine` worker thread. File I/O on the game thread adds latency between hands. Does this matter, and how would you address it?
2. `SessionMemory::load` is a static factory method rather than a constructor. What is the advantage of this design?

</details>

<details>
<summary>Answers</summary>

**Q1.** For poker — a turn-based game with natural pauses between hands — a few milliseconds of file I/O is imperceptible. If you wanted to be strict: offload the save to a background thread using `std::async(std::launch::async, [this]{ m_memory.save(...); })` and discard the future (fire-and-forget). The risk of fire-and-forget is that two concurrent saves could interleave on disk — add a mutex or use an atomic flag to ensure only one save is in flight at a time.

**Q2.** A static factory method can return a default-constructed `SessionMemory` if the file doesn't exist (first run), without the constructor needing to handle the "no file" case. It also makes the error-handling path explicit at the call site: `SessionMemory mem = SessionMemory::load(path)` clearly communicates that loading might fail, unlike `SessionMemory mem(path)` which looks like an ordinary constructor. This is the Named Constructor Idiom.

</details>

---

#### Step 4: Write tests for `SessionMemory`

- [ ] Add to `tests/ai/test_prompt_builder.cpp` (or a new `test_session_memory.cpp`):

```cpp
TEST(SessionMemoryTest, RecordsRaisesCorrectly) {
    SessionMemory mem;
    mem.recordAction(1, Action{Action::Type::Raise, 40}, Street::PreFlop, false);
    mem.recordAction(1, Action{Action::Type::Raise, 80}, Street::Flop,    false);
    mem.recordHandEnd();
    EXPECT_EQ(mem.statsFor(1).totalRaises, 2);
    EXPECT_EQ(mem.statsFor(1).preflopRaises, 1);
}

TEST(SessionMemoryTest, RecordsFoldToAggression) {
    SessionMemory mem;
    mem.recordAction(1, Action{Action::Type::Fold}, Street::Flop, /*facingRaise=*/true);
    mem.recordHandEnd();
    EXPECT_EQ(mem.statsFor(1).foldedToRaise, 1);
}

TEST(SessionMemoryTest, OpponentNotesExcludesOwnId) {
    SessionMemory mem;
    mem.recordAction(0, Action{Action::Type::Raise}, Street::PreFlop, false);
    mem.recordAction(1, Action{Action::Type::Call},  Street::PreFlop, false);
    mem.recordHandEnd();
    // Player 0 is asking — their own notes must not appear
    std::string notes = mem.formatOpponentNotes(/*ownId=*/0);
    EXPECT_EQ(notes.find("Player 0"), std::string::npos);
    EXPECT_NE(notes.find("Player 1"), std::string::npos);
}

TEST(SessionMemoryTest, PromptContainsOpponentNotesWhenMemoryProvided) {
    SessionMemory mem;
    mem.recordAction(1, Action{Action::Type::Raise}, Street::PreFlop, false);
    mem.recordAction(1, Action{Action::Type::Raise}, Street::PreFlop, false);
    mem.recordHandEnd();
    mem.recordHandEnd();
    auto view   = makeView();  // player 0 is acting
    auto prompt = PromptBuilder::build(view, "", &mem);
    EXPECT_NE(prompt.find("Opponent Notes"), std::string::npos);
}

TEST(SessionMemoryTest, PromptHasNoOpponentSectionWithoutMemory) {
    auto prompt = PromptBuilder::build(makeView(), "");
    EXPECT_EQ(prompt.find("Opponent Notes"), std::string::npos);
}

TEST(SessionMemoryTest, RoundTripSaveLoad) {
    SessionMemory mem;
    mem.recordAction(1, Action{Action::Type::Raise}, Street::PreFlop, false);
    mem.recordHandEnd();
    mem.save("/tmp/test_memory.json");

    SessionMemory loaded = SessionMemory::load("/tmp/test_memory.json");
    EXPECT_EQ(loaded.statsFor(1).totalRaises, mem.statsFor(1).totalRaises);
    EXPECT_EQ(loaded.statsFor(1).handsObserved, mem.statsFor(1).handsObserved);
}
```

- [ ] Add `test_session_memory.cpp` to `tests/ai/CMakeLists.txt` if created as a separate file.

- [ ] Commit:

```bash
git add src/ai/SessionMemory.hpp src/ai/SessionMemory.cpp
git add src/players/IPlayer.hpp src/core/GameEngine.cpp
git add tests/ai/
git commit -m "feat(ai): add SessionMemory for hand-to-hand opponent profiling"
```

---

## Evaluation

Run `/phase-verify` to compile and get automated feedback on your implementation.

### Build checklist

- [ ] `cmake --build` exits with code `0`
- [ ] Build output shows `poker_ai` library being linked
- [ ] `ctest` passes all tests including the new `ai_tests` suite
- [ ] No `undefined reference` linker errors (means nlohmann_json and httplib are linked correctly)
- [ ] Personality files exist at `config/personalities/default.md`, `aggressive.md`, `cautious.md`

### Concept checklist

- [ ] Does `ILLMClient` have `virtual ~ILLMClient() = default`?
- [ ] Is `sendPrompt` marked `= 0` (pure virtual), not just `virtual`?
- [ ] Does `OllamaClient::sendPrompt` return `""` on HTTP error, not throw?
- [ ] Does `PromptBuilder::build` take a `PlayerView`, not a `GameState + PlayerId`?
- [ ] Does `PromptBuilder::build` accept `const SessionMemory* memory = nullptr` and only inject notes when non-null?
- [ ] Does `SessionMemory::formatOpponentNotes` exclude the acting player's own ID?
- [ ] Does `IPlayer::onHandEnd` have a default no-op body so `HumanPlayer` and `MockPlayer` require no changes?
- [ ] Does `AIPlayer::onHandEnd` only record showdown hands for non-folded players?
- [ ] Does `SessionMemory` cap showdown notes per player to avoid unbounded prompt growth?
- [ ] Does the prompt omit all opponent hole cards?
- [ ] Is `yaml-cpp` absent from `src/ai/CMakeLists.txt`?
- [ ] Does `MockLLMClient` store `m_lastPrompt` for inspection in Phase 6 tests?
- [ ] Are the three personality files meaningfully distinct (not copies with the name changed)?

### Common mistakes

| Mistake | Symptom | Fix |
|---|---|---|
| `httplib::Client("http://localhost:11434")` | Runtime crash or connection refused | `httplib::Client` takes host and port separately — parse the URL first |
| `yaml-cpp::yaml-cpp` as link target | CMake error: target not found | The FetchContent target is just `yaml-cpp` (no namespace). Also: don't link it into `poker_ai` at all |
| Missing virtual destructor | Silent memory leak or UB when deleting via `ILLMClient*`; no compile warning | Add `virtual ~ILLMClient() = default` |
| `json::parse` throwing on malformed response | Game crashes when Ollama returns unexpected output | Use `json::parse(body, nullptr, false)` and check `is_discarded()` |
| `PromptBuilder` using `GameState` directly | Risk of including opponent cards in prompt | Use `PlayerView` — it only carries the acting player's hand |
| Forgetting `add_subdirectory(ai)` in `tests/CMakeLists.txt` | `ai_tests` target never registered with CTest | Add the line and reconfigure with `cmake -S . -B build` |
| `SessionMemory` recording own actions | AI builds a profile of itself, not opponents | Always pass `ownId` to `formatOpponentNotes`; skip `id == ownId` in the loop |
| Injecting all showdown notes without a cap | Prompt grows unbounded over a long session | Keep only the last N notes per player (e.g. `maxShowdownNotes = 5`) |
| Making `onHandEnd` pure virtual on `IPlayer` | `HumanPlayer`, `MockPlayer`, `NetworkPlayer` all fail to compile | Give it a default empty body — only `AIPlayer` overrides it |
| Recording folded players' hole cards at showdown | AI "learns" hands that were never revealed, corrupting its reads | Check `finalState.foldedPlayers.count(id) == 0` before recording |
