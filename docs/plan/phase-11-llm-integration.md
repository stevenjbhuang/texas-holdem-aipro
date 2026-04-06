# Phase 11 — LLM API Integration & Custom Model Server

**Concepts you'll learn:** Factory pattern, multiple interface implementations, async I/O with `std::future`, API authentication, game data collection, model fine-tuning pipelines, ELO-based model evaluation.

**Previous phase:** [Phase 10 — Online Multiplayer](phase-10-online-multiplayer.md)

**Design reminder:** Every new LLM backend is a new `ILLMClient` implementation. `AIPlayer` never changes — it only calls `sendPrompt`. The factory and config are the only place where backend selection happens.

---

## Architecture Overview

The Phase 5 AI layer grows to support multiple backends and a training pipeline:

```
                    ┌─────────────────────────────────────────────────────┐
                    │  ILLMClient                                         │
                    │  + sendPrompt(prompt) → string                      │
                    └────────┬──────────┬──────────┬──────────┬───────────┘
                             │          │          │          │
                    OllamaClient  ClaudeClient  OpenAIClient  CustomModelClient
                    (Phase 5)     (Anthropic)   (OpenAI or    (your fine-tuned
                                               compat server) model via vLLM /
                                                              llama.cpp server)

config/game.yaml
  llm:
    backend: "claude"         ← LLMClientFactory reads this
    model:   "claude-opus-4-6"
    api_key: "${ANTHROPIC_API_KEY}"

                    LLMClientFactory::create(config) → unique_ptr<ILLMClient>
```

A second track runs in parallel: a data collection and training pipeline that records real game hands, converts them to training examples, and fine-tunes a base model into a poker-specialist.

---

### Task 11.1: Implement `ClaudeClient`

The Anthropic Messages API is a REST endpoint that takes a list of messages and returns a completion. It differs from Ollama in structure (messages array instead of flat prompt) and requires an API key header.

- [ ] Create `src/ai/ClaudeClient.hpp`:

```cpp
#pragma once
#include "ILLMClient.hpp"
#include <string>

namespace poker {

class ClaudeClient : public ILLMClient {
public:
    // model: e.g. "claude-opus-4-6", "claude-sonnet-4-6", "claude-haiku-4-5-20251001"
    // apiKey: read from env var ANTHROPIC_API_KEY — never hardcode
    ClaudeClient(std::string model, std::string apiKey);

    std::string sendPrompt(const std::string& prompt) override;

private:
    std::string m_model;
    std::string m_apiKey;

    static constexpr const char* API_HOST = "api.anthropic.com";
    static constexpr int         API_PORT = 443;
};

} // namespace poker
```

- [ ] Implement `ClaudeClient.cpp`. The Anthropic API structure:

```
POST https://api.anthropic.com/v1/messages
Headers:
  x-api-key: <key>
  anthropic-version: 2023-06-01
  content-type: application/json
Body:
{
  "model": "claude-opus-4-6",
  "max_tokens": 16,
  "messages": [{"role": "user", "content": "<prompt>"}]
}
Response:
{
  "content": [{"type": "text", "text": "CALL"}],
  ...
}
```

- [ ] Read the API key from the environment — never from config files committed to git:

```cpp
// In LLMClientFactory (Task 11.3) or at construction time:
const char* key = std::getenv("ANTHROPIC_API_KEY");
if (!key || std::string(key).empty())
    throw std::runtime_error("ANTHROPIC_API_KEY not set");
```

<details>
<summary>Concepts</summary>

> **Concept: HTTPS with cpp-httplib**
>
> cpp-httplib supports HTTPS via OpenSSL. Use `httplib::SSLClient` instead of `httplib::Client`:
>
> ```cpp
> #include <httplib.h>  // compiled with CPPHTTPLIB_OPENSSL_SUPPORT
>
> httplib::SSLClient client("api.anthropic.com", 443);
> client.set_connection_timeout(5);
> client.set_read_timeout(60);   // Claude can take longer than Ollama
> ```
>
> CMake needs OpenSSL linked:
> ```cmake
> find_package(OpenSSL REQUIRED)
> target_link_libraries(poker_ai PRIVATE OpenSSL::SSL OpenSSL::Crypto)
> ```
>
> On Ubuntu: `sudo apt install libssl-dev`

> **Concept: `max_tokens` matters more than you think**
>
> The poker AI only needs to output `FOLD`, `CALL`, or `RAISE 100` — at most 3-4 tokens. Setting `max_tokens: 16` caps the response tightly, which:
> - Reduces cost (you pay per output token)
> - Prevents the model from outputting a long explanation before the action
> - Forces the model to be concise
>
> The `PromptBuilder` personality text already instructs "no explanation", but `max_tokens` enforces it at the API level.

> **Concept: Never commit API keys**
>
> API keys in source code are a serious security risk — they get scraped from git history even if later deleted. Standard practice:
> - Read from environment variables (`std::getenv`)
> - Or read from a local config file that is in `.gitignore`
> - Never from `config/game.yaml` if that file is committed
>
> Add `ANTHROPIC_API_KEY=...` to a local `.env` file and source it before running:
> ```bash
> export ANTHROPIC_API_KEY="sk-ant-..."
> ./build/texas-holdem
> ```

**Questions — think through these before checking answers:**
1. `ClaudeClient` sends a single `user` message containing the full prompt. The Anthropic API also has a `system` parameter for the system prompt. How could `PromptBuilder` be updated to split personality text into a `system` prompt and game state into the `user` message? What advantage does this have?
2. `ClaudeClient` and `OllamaClient` both parse a JSON response field to extract the model's text. In `OllamaClient` it's `response`; in `ClaudeClient` it's `content[0].text`. How could you handle the case where `content` is an empty array?

</details>

<details>
<summary>Answers</summary>

**Reference implementation (`ClaudeClient.cpp`):**

```cpp
#include "ai/ClaudeClient.hpp"
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace poker {

ClaudeClient::ClaudeClient(std::string model, std::string apiKey)
    : m_model(std::move(model)), m_apiKey(std::move(apiKey)) {}

std::string ClaudeClient::sendPrompt(const std::string& prompt) {
    httplib::SSLClient client(API_HOST, API_PORT);
    client.set_connection_timeout(5);
    client.set_read_timeout(60);

    nlohmann::json body;
    body["model"]      = m_model;
    body["max_tokens"] = 16;
    body["messages"]   = nlohmann::json::array();
    body["messages"].push_back({{"role", "user"}, {"content", prompt}});

    httplib::Headers headers = {
        {"x-api-key",           m_apiKey},
        {"anthropic-version",   "2023-06-01"},
        {"content-type",        "application/json"},
    };

    auto res = client.Post("/v1/messages", headers, body.dump(), "application/json");
    if (!res || res->status != 200) return "";

    auto parsed = nlohmann::json::parse(res->body, nullptr, false);
    if (parsed.is_discarded()) return "";

    auto& content = parsed["content"];
    if (!content.is_array() || content.empty()) return "";
    return content[0].value("text", "");
}

} // namespace poker
```

---

**Q1.** `ClaudeClient` could accept an optional system string, or `PromptBuilder` could return a struct `{systemText, userText}` instead of one flat string. The advantage: Claude follows system-prompt instructions more reliably than user-turn instructions, and the system prompt is not charged at the same rate as user tokens (in some API tiers). The downside is that `ILLMClient::sendPrompt` only takes one string — you'd either need to change the interface or encode both parts into the single string with a separator.

**Q2.** Check `content.is_array() && !content.empty()` before indexing. If the array is empty (the model produced no output, which can happen on safety refusals), return `""` so `AIPlayer` falls back to a safe default action.

</details>

---

### Task 11.2: Implement `OpenAICompatibleClient` ✅ (completed in Phase 9)

The OpenAI Chat Completions API is the de facto standard: Ollama, vLLM, llama.cpp server, LM Studio, and many others expose an OpenAI-compatible endpoint. One client implementation works for all of them.

**Already implemented** as `src/ai/OpenAICompatibleClient.hpp/.cpp`. It replaced `OllamaClient` and is now the only LLM backend. See Task 9.5.

~~- [ ] Create `src/ai/OpenAIClient.hpp`:~~

```cpp
#pragma once
#include "ILLMClient.hpp"
#include <string>

namespace poker {

// Works with: OpenAI API, vLLM, llama.cpp --server, LM Studio, Ollama /v1 endpoint
class OpenAIClient : public ILLMClient {
public:
    OpenAIClient(std::string model,
                 std::string endpoint,     // e.g. "http://localhost:8000"
                 std::string apiKey = ""); // empty for local servers

    std::string sendPrompt(const std::string& prompt) override;

private:
    std::string m_model;
    std::string m_endpoint;
    std::string m_apiKey;
};

} // namespace poker
```

- [ ] Implement using the `/v1/chat/completions` endpoint:

```
POST /v1/chat/completions
Headers: Authorization: Bearer <apiKey>  (omit if empty)
Body:
{
  "model": "your-model",
  "max_tokens": 16,
  "messages": [
    {"role": "system", "content": "<personality>"},
    {"role": "user",   "content": "<game state>"}
  ]
}
Response: { "choices": [{"message": {"content": "CALL"}}] }
```

- [ ] Update `PromptBuilder` to optionally return a `{systemText, userText}` split so `OpenAIClient` and `ClaudeClient` can use the structured message format:

```cpp
struct PromptParts {
    std::string systemText;  // personality
    std::string userText;    // game state
};
static PromptParts buildParts(const PlayerView& view,
                              const std::string& personalityText);
// build() (existing) just concatenates them for Ollama
```

<details>
<summary>Concepts</summary>

> **Concept: OpenAI-compatible servers**
>
> The OpenAI Chat Completions format became an industry standard. Running a local model with an OpenAI-compatible interface means you can swap between:
>
> | Backend | Launch command | Endpoint |
> |---|---|---|
> | llama.cpp server | `./server -m model.gguf --port 8080` | `http://localhost:8080` |
> | vLLM | `python -m vllm.entrypoints.openai.api_server --model ...` | `http://localhost:8000` |
> | LM Studio | GUI, built-in server | `http://localhost:1234` |
> | Ollama (v1 compat) | `ollama serve` | `http://localhost:11434/v1` |
> | OpenAI cloud | n/a | `https://api.openai.com` |
>
> `OpenAIClient` works with all of these by just changing `endpoint` and `apiKey` in `config/game.yaml`.

> **Concept: System vs user role in chat models**
>
> Chat-tuned models are trained on conversations with explicit roles. The `system` role sets persistent instructions (persona, output format); the `user` role provides the current input. Using them correctly improves instruction-following:
>
> ```json
> {"role": "system", "content": "You are an aggressive poker player. Respond with FOLD/CALL/RAISE only."}
> {"role": "user",   "content": "Street: Flop. Pot: $40. Your hand: As Kh. ..."}
> ```
>
> For flat-prompt models (Ollama `/api/generate`), concatenation is the fallback.

**Questions — think through these before checking answers:**
1. `OpenAIClient` and `OllamaClient` both talk to `localhost` by default. Could `OpenAIClient` completely replace `OllamaClient` (since Ollama exposes `/v1/chat/completions` at `/v1`)? What would be the downside of doing so?
2. `apiKey` defaults to `""` for local servers. How should the `Authorization: Bearer` header be handled when the key is empty — include the header with an empty value, or omit it entirely?

</details>

<details>
<summary>Answers</summary>

**Q1.** Yes, `OpenAIClient` with endpoint `http://localhost:11434/v1` can replace `OllamaClient`. The downside is losing Ollama-specific features: the native `/api/generate` endpoint supports Ollama-specific options (temperature, top-k, system prompt field, context length) that `/v1/chat/completions` may not expose. For basic use, replacing is fine; for advanced tuning, keeping `OllamaClient` for Ollama-specific configuration is worthwhile.

**Q2.** Omit the header entirely when `apiKey` is empty. Many local servers reject or warn on `Authorization: Bearer ` (empty value). A header with an empty bearer token could also trigger a `401 Unauthorized` on servers that validate it. Check with `if (!m_apiKey.empty())` before adding the header to the request.

</details>

---

### Task 11.3: Implement `LLMClientFactory`

The factory reads `config/game.yaml` and constructs the right `ILLMClient` implementation. This is the only place in the codebase where concrete client types are named.

- [ ] Update `config/game.yaml`:

```yaml
llm:
  backend: "ollama"               # ollama | claude | openai | custom
  model:   "llama3.2"
  endpoint: "http://localhost:11434"
  api_key_env: "ANTHROPIC_API_KEY"  # env var to read key from (blank = no auth)
  timeout_s: 30

game:
  num_players: 4
  starting_stack: 1000
  small_blind: 5
  big_blind: 10
```

- [ ] Create `src/ai/LLMClientFactory.hpp`:

```cpp
#pragma once
#include "ILLMClient.hpp"
#include <memory>
#include <string>

namespace poker {

class LLMClientFactory {
public:
    // Reads config/game.yaml and constructs the appropriate ILLMClient.
    // Throws std::runtime_error if backend is unknown or required env var is missing.
    static std::unique_ptr<ILLMClient> create(const std::string& configPath);
};

} // namespace poker
```

- [ ] Implement using yaml-cpp (belongs here, not in `poker_ai` — see Phase 5 CMake notes):

```cpp
// In main.cpp or a dedicated config loader:
#include <yaml-cpp/yaml.h>

auto node   = YAML::LoadFile(configPath);
auto backend = node["llm"]["backend"].as<std::string>();

if (backend == "claude") {
    auto key = getEnvOrThrow(node["llm"]["api_key_env"].as<std::string>());
    return std::make_unique<ClaudeClient>(model, key);
} else if (backend == "openai" || backend == "custom") {
    return std::make_unique<OpenAIClient>(model, endpoint, key);
} else { // default: ollama
    return std::make_unique<OllamaClient>(model, endpoint);
}
```

<details>
<summary>Concepts</summary>

> **Concept: The Factory pattern**
>
> A factory centralises construction decisions. The caller asks for an `ILLMClient` and doesn't need to know which concrete type it gets:
>
> ```cpp
> // main.cpp — no concrete types mentioned
> auto client = LLMClientFactory::create("config/game.yaml");
> auto aiPlayer = std::make_unique<AIPlayer>(id, client.get(), personality);
> ```
>
> Without a factory, `main.cpp` would have a long `if/else` block with `#include`s for every backend. The factory moves that complexity to one place and makes adding a new backend a one-file change.

> **Concept: Config-driven vs code-driven selection**
>
> Changing the backend requires editing `config/game.yaml`, not recompiling. This matters for deployment: you can ship one binary and let users configure their preferred backend (local Ollama, Claude API, custom server) via a text file.
>
> The pattern extends to model selection, endpoint URLs, timeouts, and personalities — all things users might want to change without touching code.

> **Concept: Where yaml-cpp belongs**
>
> As established in Phase 5, `yaml-cpp` must not be linked into `poker_ai`. `LLMClientFactory` uses yaml-cpp to read the config. Two options:
> - Put `LLMClientFactory` in a separate `config/` layer that links both `poker_ai` and `yaml-cpp`
> - Implement the factory in `main.cpp` directly (simplest for this project size)
>
> For this project, implementing the factory logic in `main.cpp` and having it call `LLMClientFactory::create()` — which takes already-parsed values rather than a file path — keeps `yaml-cpp` in `main.cpp` only.

**Questions — think through these before checking answers:**
1. `LLMClientFactory::create` throws if the backend name is unknown. What is the alternative, and why is throwing better here than returning `nullptr`?
2. The factory reads `api_key_env` from config, then calls `std::getenv` with that value. Why store the *name* of the env var in config rather than the key itself?

</details>

<details>
<summary>Answers</summary>

**Q1.** Returning `nullptr` pushes error handling to the caller — every call site must null-check before use, and forgetting causes a null-pointer dereference deep in gameplay. Throwing at construction time surfaces the misconfiguration immediately at startup, before any game logic runs. The error message can explain exactly what's wrong (`"Unknown backend: gpt5 — expected ollama, claude, openai, or custom"`), whereas a null deref gives no context.

**Q2.** Storing the key value itself in a committed config file is a security risk — it ends up in git history. Storing the env var *name* (`"ANTHROPIC_API_KEY"`) is safe to commit: it tells the program where to look for the key, without containing the key itself. The actual secret stays in the shell environment, a `.env` file in `.gitignore`, or a secrets manager.

</details>

---

### Task 11.4: Make LLM calls async

Currently `AIPlayer::getAction` blocks the `GameEngine` worker thread while waiting for the HTTP response. For a local game this is fine. For online play (Phase 10), a slow AI blocks all other clients.

- [ ] Update `AIPlayer::getAction` to launch the HTTP call on a `std::async` thread and join with a timeout:

```cpp
Action AIPlayer::getAction(const PlayerView& view) {
    std::string prompt = PromptBuilder::build(view, m_personality);

    auto future = std::async(std::launch::async,
        [&]() { return m_client->sendPrompt(prompt); });

    using namespace std::chrono_literals;
    if (future.wait_for(m_timeoutMs) == std::future_status::timeout) {
        return Action{Action::Type::Fold};  // safe fallback
    }

    return parseAction(future.get());
}
```

- [ ] Add a `timeout_ms` field to `AIPlayer` (read from config via `GameConfig` or a new `AIConfig`).

<details>
<summary>Concepts</summary>

> **Concept: `std::async` vs `std::thread`**
>
> `std::async(std::launch::async, f)` launches `f` on a thread pool (or new thread) and returns a `std::future<T>`. It is simpler than `std::thread` for one-shot tasks:
>
> ```cpp
> // With std::thread — must manage lifetime manually
> std::string result;
> std::thread t([&]() { result = client->sendPrompt(prompt); });
> t.join();
>
> // With std::async — future manages lifetime
> auto fut = std::async(std::launch::async, [&]() {
>     return client->sendPrompt(prompt);
> });
> std::string result = fut.get();  // blocks until done
> ```
>
> The `std::launch::async` flag forces immediate thread creation. Without it, the implementation is allowed to defer execution until `.get()` is called (lazy evaluation), which would defeat the purpose.

> **Concept: Timeout behaviour in a turn-based game**
>
> Unlike real-time games, poker has natural time pressure — the "shot clock". The timeout serves two purposes:
> 1. **Correctness:** prevents the game from hanging if the LLM server is down
> 2. **Gameplay:** mirrors real poker time limits
>
> The fallback action on timeout should be conservative (Fold or Check) rather than aggressive (Raise), since a timeout likely indicates something went wrong, not a bold play.

**Questions — think through these before checking answers:**
1. `std::async` with `std::launch::async` always creates a new thread. For a 4-player AI game, up to 4 HTTP calls could launch concurrently (if `GameEngine` somehow prompted multiple players simultaneously — which it doesn't, but consider). Is this a concern?
2. The lambda captures `prompt` by reference. `prompt` is a local variable in `getAction`. If `getAction` returns early (e.g. timeout), is the reference still valid when the async thread tries to use it?

</details>

<details>
<summary>Answers</summary>

**Q1.** In the current design, `GameEngine` calls `getAction` sequentially — one player at a time — so at most one async thread is active at any moment. Concurrent calls are not a concern with the current architecture. If you later parallelise (e.g. gather all actions simultaneously for speed), thread-pool pressure and rate limiting become real concerns.

**Q2.** This is a use-after-free bug. If `getAction` returns on timeout before the async thread finishes, `prompt` is destroyed. The async thread then reads dangling memory. **Fix:** capture `prompt` by value in the lambda: `[prompt = std::move(prompt)]() { return client->sendPrompt(prompt); }`. Moving into the lambda gives the thread its own copy that lives as long as the lambda.

</details>

---

### Task 11.5: Implement `GameLogger` for training data collection

Every hand played is a training example. `GameLogger` records the full game history in a format suitable for fine-tuning.

- [ ] Create `src/core/GameLogger.hpp`:

```cpp
#pragma once
#include "GameState.hpp"
#include "Types.hpp"
#include <fstream>
#include <string>

namespace poker {

// Appends one JSON record per hand to a newline-delimited JSON file (JSONL).
// Each record: { "hand": [...actions...], "winner": id, "pot": n }
class GameLogger {
public:
    explicit GameLogger(const std::string& path);

    void logAction(const GameState& state, PlayerId actor, Action action);
    void logHandEnd(PlayerId winner, int pot);
    void flush();

private:
    std::ofstream m_file;
    nlohmann::json m_currentHand;
};

} // namespace poker
```

- [ ] Integrate into `GameEngine` — add an optional `GameLogger*` (nullable — logging is off by default):

```cpp
// GameEngine.hpp
void setLogger(GameLogger* logger) { m_logger = logger; }

// GameEngine.cpp — in runBettingRound, after applying each action:
if (m_logger)
    m_logger->logAction(m_state, activeId, action);
```

- [ ] Log format (JSONL — one JSON object per line):

```json
{"street":"PreFlop","actor":2,"action":"RAISE","amount":40,"pot":45,"community":[]}
{"street":"PreFlop","actor":0,"action":"FOLD","amount":0,"pot":45,"community":[]}
{"street":"PreFlop","actor":1,"action":"CALL","amount":40,"pot":85,"community":[]}
{"hand_end":true,"winner":1,"pot":85}
```

<details>
<summary>Concepts</summary>

> **Concept: JSONL (newline-delimited JSON)**
>
> JSONL stores one JSON object per line. It is the standard format for ML training datasets because:
> - Streamable: you can append one record without rewriting the whole file
> - Parseable line-by-line: `for line in open("hands.jsonl"): json.loads(line)`
> - Compatible with most fine-tuning frameworks (Hugging Face, OpenAI fine-tune API, LLaMA-Factory)
>
> The alternative (one large JSON array) requires loading the entire file into memory to append or parse.

> **Concept: Nullable pointer for optional behaviour**
>
> `GameEngine` holds `GameLogger* m_logger = nullptr`. When no logger is set, every `if (m_logger)` check is false — zero overhead, no logging. When set, logging activates.
>
> This is simpler than a compile-time flag or a separate `LoggingGameEngine` subclass. The pattern is sometimes called "optional observer" — the engine doesn't know or care whether anyone is watching.

**Questions — think through these before checking answers:**
1. `GameLogger` writes to a file. `GameEngine`'s worker thread calls `logAction`. The SFML main thread does not touch the logger. Is there a thread safety concern?
2. What additional fields would make the logged data more useful for training? Think about what a supervised model needs to learn to play poker well.

</details>

<details>
<summary>Answers</summary>

**Q1.** No — `GameLogger` is only ever called from the `GameEngine` worker thread (all `logAction` calls originate from `runBettingRound`). No other thread touches it, so no mutex is needed. If you later add logging from the renderer (e.g. to log UI events), that would require synchronisation.

**Q2.** Useful additions: hole cards of the acting player (so the model can learn which hands to fold), hole cards of the winner revealed at showdown (so the model can learn what winning hands look like), pot odds at the moment of decision (`callCost / pot`), position relative to dealer (early/middle/late), and the outcome label (did the acting player win the hand?). The outcome is the training signal: actions that led to winning hands are positive examples; actions that led to losing hands are negative.

</details>

---

### Task 11.6: Training data pipeline

- [ ] Write a Python script `scripts/prepare_training_data.py` that converts `hands.jsonl` into a fine-tuning dataset:

```python
# For each action in the log, construct a (prompt, completion) pair:
# prompt = what PromptBuilder would have generated for that state
# completion = the action taken
#
# Filter to only include winning hands (or weight winning actions higher).
# Output format: Hugging Face datasets / OpenAI fine-tune JSONL:
# {"messages": [
#     {"role": "system", "content": "<personality>"},
#     {"role": "user",   "content": "<game state>"},
#     {"role": "assistant", "content": "RAISE 80"}
# ]}
```

- [ ] Collect at least 1,000 hands of AI-vs-AI gameplay using `GameLogger`:

```bash
# Run a headless simulation (no SFML window) for data collection
./build/texas-holdem --headless --hands 1000 --log hands.jsonl
```

- [ ] Process and inspect the dataset:

```bash
python scripts/prepare_training_data.py --input hands.jsonl --output training_data.jsonl
wc -l training_data.jsonl   # expect ~5-10 action records per hand
```

<details>
<summary>Concepts</summary>

> **Concept: Supervised fine-tuning (SFT)**
>
> SFT trains a model to imitate a set of (input, output) examples. In this case:
> - Input: the poker prompt (game state + personality)
> - Output: the action taken by a good player
>
> The model learns the mapping from game state to action by gradient descent on the cross-entropy loss between its predicted tokens and the target tokens.
>
> The quality of the training data determines the ceiling of the fine-tuned model's skill. If you train on actions from a poor baseline AI, the fine-tuned model will also play poorly — but consistently.

> **Concept: Data quality over quantity**
>
> 1,000 hands of mediocre AI play may produce a worse fine-tuned model than 100 hands of expert play. Options for higher-quality data:
> - Filter to hands where the acting player eventually won
> - Generate data from a stronger baseline (GPT-4, Claude Opus prompting)
> - Use hand history files from online poker sites (check licensing)
> - Implement a simple equity-based oracle player to generate "correct" actions

> **Concept: Headless mode**
>
> A `--headless` flag skips SFML entirely and runs only `GameEngine` in a tight loop. This lets you collect thousands of hands per second for data generation, without opening a window. Implement by conditionally constructing the renderer in `main.cpp` based on the flag.

**Questions — think through these before checking answers:**
1. The dataset contains the acting player's hole cards (logged in `logAction`). During training, should the model see these? During inference (actual gameplay), can it see them?
2. Fine-tuning on actions from the current AI (which itself was prompted with a personality) creates a feedback loop. What is the risk, and how do you break it?

</details>

<details>
<summary>Answers</summary>

**Q1.** Yes, the model should see the hole cards during both training and inference. The hole cards are part of `PlayerView` and are included in the prompt by `PromptBuilder` — the model already sees them in the live game. Training and inference must use identical inputs; including cards during training but not inference (or vice versa) would create a train/inference mismatch that degrades performance.

**Q2.** The risk is mode collapse: the fine-tuned model learns to mimic the original AI's style, including its mistakes. If the original AI folds too aggressively pre-flop, the fine-tuned model will also fold too aggressively. To break the loop: generate some training data from a stronger oracle (equity calculator), filter training examples to only winning actions, or use RLHF — where the reward signal is chip EV rather than imitation of the original AI.

</details>

---

### Task 11.7: Deploy a custom model server

- [ ] Fine-tune a base model using your training data. The simplest path uses the Hugging Face `transformers` + `trl` library:

```bash
pip install trl transformers datasets
python scripts/finetune.py \
    --base-model "unsloth/Llama-3.2-3B-Instruct" \
    --dataset training_data.jsonl \
    --output-dir models/poker-llama-v1 \
    --epochs 3
```

- [ ] Export to GGUF format for llama.cpp:

```bash
python llama.cpp/convert-hf-to-gguf.py models/poker-llama-v1 \
    --outfile models/poker-llama-v1.gguf \
    --outtype q4_k_m   # 4-bit quantisation for speed
```

- [ ] Start the llama.cpp server (OpenAI-compatible):

```bash
./llama.cpp/server \
    -m models/poker-llama-v1.gguf \
    --port 8080 \
    --ctx-size 2048
```

- [ ] Update `config/game.yaml` to point at the custom server:

```yaml
llm:
  backend: "openai"
  model:   "poker-llama-v1"
  endpoint: "http://localhost:8080"
  api_key_env: ""
```

<details>
<summary>Concepts</summary>

> **Concept: Quantisation**
>
> Full-precision (fp32) models are memory-intensive. Quantisation reduces each weight to fewer bits:
>
> | Format | Bits/weight | RAM (3B model) | Quality loss |
> |---|---|---|---|
> | fp32 | 32 | ~12 GB | none (baseline) |
> | fp16 | 16 | ~6 GB | negligible |
> | q8_0 | 8 | ~3 GB | very small |
> | q4_k_m | ~4.5 | ~2 GB | small |
> | q2_k | ~2.5 | ~1.2 GB | noticeable |
>
> `q4_k_m` is the standard trade-off for local inference: roughly half the RAM of fp16 with minimal quality degradation on instruction-following tasks. For poker action prediction (3-token outputs), even `q2_k` may be acceptable.

> **Concept: The llama.cpp server OpenAI compatibility layer**
>
> `./server` from the llama.cpp project starts an HTTP server that speaks the OpenAI Chat Completions API. Your `OpenAIClient` connects to it without any changes — only the `endpoint` and `model` name in config need updating. This is the "write once, run anywhere" value of the `OpenAIClient` abstraction.

**Questions — think through these before checking answers:**
1. Fine-tuning on 5,000 action examples takes minutes on a GPU but hours on a CPU. What do you do if you don't have a GPU available?
2. After deploying the custom model, how do you know if it plays better than the baseline Llama 3.2 prompted with the personality file?

</details>

<details>
<summary>Answers</summary>

**Q1.** Use a cloud GPU notebook (Google Colab free tier, Kaggle kernels) for the fine-tuning step, then download the GGUF file to your machine for inference. Alternatively, use the OpenAI fine-tuning API (`openai.FineTuningJob.create`) — it handles GPU allocation and returns a model ID you can call via `OpenAIClient`.

**Q2.** Run an automated ELO tournament (Task 11.8): simulate games where the fine-tuned model plays against the baseline model and record win rates. After enough hands, the ELO difference quantifies skill. A simpler check: compare average pot won per hand and aggression frequency (raise%) — if the fine-tuned model raises with stronger hands and folds weaker ones, it has learned something.

</details>

---

### Task 11.8: ELO evaluation system

- [ ] Write `scripts/evaluate.py` — a tournament runner that pits two `ILLMClient` configurations against each other across N simulated hands and computes an ELO rating:

```python
# Headless simulation: Player A (baseline) vs Player B (fine-tuned)
# Run 200 hands, record chip delta per player per hand
# ELO update: standard 32-K factor, expected score from current ratings
# Output: rating history plot + final ELO differential
```

- [ ] Run the evaluation and record the baseline vs fine-tuned ELO differential in `docs/results/model-eval.md`.

- [ ] Automate evaluation in `scripts/coverage.sh` (or a new `scripts/eval.sh`) so it runs after each fine-tuning iteration.

---

## Evaluation

### Build checklist

- [ ] `cmake --build` exits with code `0` with all three new clients compiled
- [ ] `ctest` passes — existing tests unaffected
- [ ] `ClaudeClient` connects to the Anthropic API and returns a valid action (requires `ANTHROPIC_API_KEY` set)
- [ ] `OpenAIClient` connects to a local llama.cpp server and returns a valid action
- [ ] `LLMClientFactory::create` constructs the correct client type from `config/game.yaml`
- [ ] `GameLogger` produces a valid JSONL file after a 10-hand session

### Concept checklist

- [ ] Does `LLMClientFactory` contain the only `#include`s for concrete client types (`ClaudeClient.hpp`, `OpenAIClient.hpp`)? Does `AIPlayer` remain unaware of them?
- [ ] Does `ClaudeClient` read its API key from an environment variable, not from config?
- [ ] Does `OpenAIClient` omit the `Authorization` header when `apiKey` is empty?
- [ ] Does the async timeout in `AIPlayer::getAction` capture `prompt` by value (not by reference)?
- [ ] Does `GameLogger` only ever write from the `GameEngine` worker thread?
- [ ] Does `GameState` serialisation live outside `src/core/` (no nlohmann dependency in core)?

### Common mistakes

| Mistake | Symptom | Fix |
|---|---|---|
| API key in `config/game.yaml` (committed to git) | Key exposed in git history permanently | Use `api_key_env` to name the env var; read with `std::getenv` |
| `std::async` lambda captures `prompt` by reference | Use-after-free if timeout fires before thread finishes | Capture by value: `[prompt = std::move(prompt)]() { ... }` |
| `yaml-cpp` linked into `poker_ai` | `poker_ai` gains a config-parsing dependency | Keep yaml-cpp in `main.cpp` or a separate config layer |
| Fine-tuning on all actions including losing plays | Model learns to mimic losses | Filter training data to winning-player actions, or add outcome weighting |
| Missing `std::launch::async` flag on `std::async` | Async call may defer until `.get()` — identical to synchronous | Always specify `std::launch::async` explicitly |
| Sending full `GameState` to Claude (includes all hole cards) | Model plays with perfect information | Always build prompt from `PlayerView`, never `GameState` directly |
