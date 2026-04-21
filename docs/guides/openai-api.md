# OpenAI-Compatible API

Both **Ollama** and **llama.cpp** (and many other LLM servers) implement the
OpenAI HTTP API standard. This project targets that standard exclusively via
`OpenAICompatibleClient`, so any compliant server works without code changes —
only `config/game.yaml` needs updating.

---

## Endpoints used

### `GET /v1/models`

Health and model availability checks.

**Response:**
```json
{
  "data": [
    { "id": "llama3.2" },
    { "id": "qwen3" }
  ]
}
```

Used by `isServerAvailable()` (checks HTTP 200) and `isModelAvailable()` (checks
that `LLMConfig::model` appears in `data[].id`).

---

### `POST /v1/chat/completions`

Inference. Sends a list of messages by role and receives a reply.

**Request:**
```json
{
  "model": "llama3.2",
  "stream": false,
  "messages": [
    { "role": "system", "content": "You are an aggressive poker player..." },
    { "role": "user",   "content": "## Current game state:\n..." }
  ]
}
```

**Response:**
```json
{
  "choices": [
    {
      "message": {
        "role": "assistant",
        "content": "RAISE 120"
      }
    }
  ]
}
```

`sendPrompt()` returns `choices[0]["message"]["content"]`, or `""` on failure.

---

## Message roles

| Role | Purpose | Source in this project |
|------|---------|----------------------|
| `system` | Persistent persona / instructions | AI player personality file |
| `user` | The current prompt | `PromptBuilder::build()` output |
| `assistant` | Model's reply | Parsed by `AIPlayer::parseResponse()` |

Separating personality into the `system` role keeps it out of the game state
prompt and lets the model treat it as standing instructions rather than
conversational context.

---

## Switching backends

Set `llm.backend` in `config/game.yaml` to point to the desired backend file:

```yaml
# config/game.yaml
llm:
  backend: "config/backends/ollama.yaml"   # or llamacpp.yaml, openai.yaml
```

Each backend file contains its own connection settings:

| File | Endpoint | API key |
|------|----------|---------|
| `backends/ollama.yaml` | `http://localhost:11434` | not required |
| `backends/llamacpp.yaml` | `http://localhost:8080` | not required |
| `backends/openai.yaml` | `https://api.openai.com` | required |

The `model` field must match the model name as reported by `/v1/models` on that server.

HTTPS endpoints (anything starting with `https://`) are handled automatically —
`OpenAICompatibleClient` constructs an SSL connection. Requires `libssl-dev`
at build time (`sudo apt install libssl-dev`).

---

## `LLMConfig`

```cpp
struct LLMConfig {
    std::string model;             // e.g. "llama3.2" or "gpt-4o"
    std::string endpoint;          // e.g. "http://localhost:11434" or "https://api.openai.com"
    std::string apiKey;            // optional; sent as "Authorization: Bearer <key>"
    int  connectionTimeout = 5;    // seconds
    int  readTimeout       = 30;   // seconds
};
```

Defined in `src/ai/ILLMClient.hpp` so it is available to any future client
implementation without depending on a concrete class.
