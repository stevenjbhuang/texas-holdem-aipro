# Phase 5 — AI Layer: ILLMClient, OllamaClient, PromptBuilder

**Concepts you'll learn:** Interface design (abstract base classes), dependency injection, HTTP clients, JSON parsing, prompt engineering.

**Previous phase:** [Phase 4 — Core Tests](phase-4-core-tests.md)
**Next phase:** [Phase 6 — Players Layer](phase-6-players-layer.md)

---

### Task 5.1: Understand interface design in C++

**Concept:** A pure abstract class (interface) in C++ declares virtual methods with `= 0`. Any class that inherits from it MUST implement those methods. This lets you swap implementations without changing the calling code.

```cpp
class ILLMClient {
public:
    virtual ~ILLMClient() = default;                         // always virtual destructor
    virtual std::string sendPrompt(const std::string& prompt) = 0;  // pure virtual
};

class OllamaClient : public ILLMClient {
public:
    std::string sendPrompt(const std::string& prompt) override { /* ... */ }
};

class MockLLMClient : public ILLMClient {
public:
    std::string sendPrompt(const std::string& prompt) override {
        return "CALL";  // always calls — for testing
    }
};
```

**Your turn:** Why does `ILLMClient` need a virtual destructor? What happens without it?

---

### Task 5.2: Create `ILLMClient`

- [ ] Create `src/ai/ILLMClient.hpp` — you know the interface already from Task 5.1. Write it.

---

### Task 5.3: Create personality config files

Before implementing `PromptBuilder`, create the personality files it will read.

- [ ] Create `config/personalities/default.md`:

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

- [ ] Create `config/personalities/aggressive.md` — write your own. Make this player raise often, bluff occasionally, rarely fold.

- [ ] Create `config/personalities/cautious.md` — conservative player, folds marginal hands, rarely bluffs.

---

### Task 5.4: Implement `PromptBuilder`

**Concept:** This class serializes `GameState` into a human-readable prompt. The quality of this class directly affects how well the AI plays.

- [ ] Create `src/ai/PromptBuilder.hpp`:

```cpp
#pragma once
#include "../core/GameState.hpp"
#include <string>

namespace poker {

class PromptBuilder {
public:
    // Builds a complete prompt for the given player.
    // personalityText: contents of the personality .md file
    static std::string build(const GameState& state,
                             PlayerId ownId,
                             const std::string& personalityText);

private:
    static std::string formatCards(const std::vector<Card>& cards);
    static std::string formatHand(const Hand& hand);
    static std::string formatBettingHistory(const GameState& state);
};

} // namespace poker
```

- [ ] Implement `src/ai/PromptBuilder.cpp`. **Your turn.** The prompt should include:
  1. Personality text (injected at top)
  2. Your hole cards
  3. Community cards (with street name)
  4. Pot size
  5. Each player's chip count and current bet
  6. Legal actions: `FOLD`, `CALL $X`, `RAISE $Y-$Z`
  7. Decision instruction from personality file

**Important:** Do NOT include other players' hole cards in the prompt. Only `state.holeCards[ownId]`.

---

### Task 5.5: Implement `OllamaClient`

**Concept:** `cpp-httplib` is a single-header HTTP library. Ollama exposes a REST API at `localhost:11434`.

Ollama's generate endpoint:
```
POST http://localhost:11434/api/generate
Body: { "model": "llama3.2", "prompt": "...", "stream": false }
Response: { "response": "CALL", ... }
```

- [ ] Create `src/ai/OllamaClient.hpp`:

```cpp
#pragma once
#include "ILLMClient.hpp"
#include <string>

namespace poker {

class OllamaClient : public ILLMClient {
public:
    OllamaClient(const std::string& model, const std::string& endpoint);

    std::string sendPrompt(const std::string& prompt) override;

private:
    std::string m_model;
    std::string m_endpoint;  // e.g. "http://localhost:11434"
};

} // namespace poker
```

- [ ] Implement `src/ai/OllamaClient.cpp`. **Your turn.** Hints:
  - `#include <httplib.h>` for cpp-httplib
  - `#include <nlohmann/json.hpp>` for JSON
  - Parse `m_endpoint` to split host and port for httplib's `Client`
  - On HTTP error, return empty string (caller handles fallback)

- [ ] Create `src/ai/CMakeLists.txt`:

```cmake
add_library(ai STATIC
    OllamaClient.cpp
    PromptBuilder.cpp
)

target_include_directories(ai PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(ai PUBLIC
    core
    nlohmann_json::nlohmann_json
    httplib::httplib
    # yaml-cpp belongs in main.cpp (config loading), not here
    # Note: cpp-httplib exports httplib::httplib as of v0.11+.
    # If you see a "target not found" error, use this fallback instead:
    #   target_include_directories(ai PUBLIC ${httplib_SOURCE_DIR})
)
```

**Note on `yaml-cpp`:** The FetchContent target for yaml-cpp is `yaml-cpp` (not `yaml-cpp::yaml-cpp`). It is not linked into the `ai` library because `OllamaClient` and `PromptBuilder` don't parse YAML — only `main.cpp` does. We'll link it there in Phase 8.

- [ ] Add `add_subdirectory(ai)` to `src/CMakeLists.txt`

- [ ] Build:
```bash
cmake --build build -j$(nproc)
```

- [ ] Commit:
```bash
git add -A && git commit -m "feat(ai): add ILLMClient, OllamaClient, PromptBuilder, personality configs"
```

---

## Evaluation

Run `/phase-verify` to compile and get automated feedback on your implementation.

### Build checklist

```bash
# 1. Configure (if not already done, or after new CMakeLists files were added)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug

# 2. Build — the ai static library must compile cleanly
cmake --build build -j$(nproc)
```

Success looks like:
- `cmake --build` exits with code `0`
- The build output includes a line compiling `libai.a` (or similar), confirming the `ai` library target was built
- No `undefined reference` errors — means `nlohmann_json` and `httplib` were linked correctly
- The personality config files exist at their expected paths:
  - `config/personalities/default.md`
  - `config/personalities/aggressive.md`
  - `config/personalities/cautious.md`

If you see `No rule to make target 'ai'`, confirm `add_subdirectory(ai)` is present in `src/CMakeLists.txt`. If httplib linking fails, try the fallback `target_include_directories` noted in the CMakeLists scaffold.

### Concept checklist

- [ ] Does `ILLMClient` have a virtual destructor declared as `virtual ~ILLMClient() = default`?
- [ ] Is `sendPrompt` declared as a pure virtual method (`= 0`) in `ILLMClient`, not just `virtual`?
- [ ] Does `OllamaClient` use `override` on `sendPrompt` (not just re-declare it identically)?
- [ ] Does `OllamaClient::sendPrompt` return an empty string on HTTP error rather than throwing?
- [ ] Does `PromptBuilder::build` omit other players' hole cards — only `state.holeCards[ownId]` is included?
- [ ] Is `PromptBuilder::build` a `static` method (no instance required to call it)?
- [ ] Are `yaml-cpp` and SFML headers absent from `src/ai/` — does the `ai` library have zero dependency on them?
- [ ] Did you write distinct personality files (not just copies of `default.md` with the name changed)?

### Common mistakes

| Mistake | Symptom | Fix |
|---|---|---|
| Using `yaml-cpp::yaml-cpp` as the link target | CMake error: `target 'yaml-cpp::yaml-cpp' not found` | The FetchContent target is `yaml-cpp` (no namespace). Also: yaml-cpp belongs in `main.cpp`, not in the `ai` library at all |
| Missing virtual destructor on `ILLMClient` | Deleting an `OllamaClient` via an `ILLMClient*` silently leaks memory or causes UB; no compile error | Always add `virtual ~ILLMClient() = default` to every interface class |
| Including other players' hole cards in the prompt | AI has perfect information and always plays optimally — the game is not interesting to play against | Only access `state.holeCards[ownId]` inside `PromptBuilder::build`; other entries must not appear in the output string |
| Parsing `m_endpoint` incorrectly for httplib's `Client` | Runtime crash or `httplib::Client` connects to wrong host/port | `httplib::Client` takes host and port separately; split on `:` and parse the port as an integer |
| Linking `httplib` or `nlohmann_json` with wrong target name | CMake error: `target 'httplib' not found` | Check the FetchContent target names in `cmake/Dependencies.cmake`; use the fallback include directory method if the exported target name differs |

### Self-score

- **Solid**: `libai.a` compiled first try. You can explain why a virtual destructor is necessary and why `yaml-cpp` is excluded from this library. Prompt output is readable and omits opponents' cards.
- **Learning**: Built after 1-2 CMake target name fixes or a link error. You looked up how to split a URL string or how `httplib::Client` takes its arguments. Most checklist boxes checked after a second pass.
- **Needs review**: Multiple linker errors, wrong target names, or the `ai` subdirectory was never registered. Go back to Task 5.1, re-read the interface design concept box and the CMake notes in `CLAUDE.md`, then revisit the `src/ai/CMakeLists.txt` scaffold carefully before moving to Phase 6.
