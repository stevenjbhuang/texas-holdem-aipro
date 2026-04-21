# Phase 10 — Online Multiplayer

**Concepts you'll learn:** Client-server architecture, network protocols, message serialisation, authoritative game servers, connection lifecycle management, concurrency across the network boundary.

**Previous phase:** [Phase 9 — Polish & AI Tuning](phase-9-polish.md)
**Next phase:** [Phase 11 — LLM API & Custom Model Server](phase-11-llm-integration.md)

**Design reminder:** `GameEngine` stays entirely on the server. Clients never run game logic — they only render state and submit player actions. The server is the single source of truth.

---

## Architecture Overview

The current single-process architecture becomes a client-server split:

```
Before (single process):
┌─────────────────────────────────────────────────┐
│  main thread: SFML renderer                     │
│  worker thread: GameEngine ↔ IPlayer instances  │
└─────────────────────────────────────────────────┘

After (client-server):
┌──────────────────────────────┐     TCP/WebSocket
│  SERVER                      │◄───────────────────►┐
│  GameEngine (worker thread)  │                      │
│  NetworkPlayer × N           │◄──────────────────►  │
│  GameServer (listener)       │                    ┌─┴────────────────────┐
└──────────────────────────────┘                    │  CLIENT              │
                                                    │  SFML renderer       │
                                                    │  HumanPlayer / AI    │
                                                    │  GameClient          │
                                                    └──────────────────────┘
```

`NetworkPlayer` is the key bridge: it implements `IPlayer` so `GameEngine` has no idea it's talking over a network. When `getAction()` is called, `NetworkPlayer` sends a `ACTION_REQUEST` message to the client and blocks until the client responds with `ACTION_RESPONSE`.

---

### Task 10.1: Design the message protocol

Before writing any code, define every message type the server and client will exchange. A clear protocol prevents ambiguity and makes both sides independently testable.

- [ ] Create `src/network/Protocol.hpp` with all message types:

```cpp
#pragma once
#include "core/GameState.hpp"
#include "core/Types.hpp"
#include <string>
#include <nlohmann/json.hpp>

namespace poker::net {

enum class MessageType {
    // Server → Client
    GameStateUpdate,   // full GameState snapshot after each change
    ActionRequest,     // server is waiting for this client's action
    GameOver,          // hand or game has ended

    // Client → Server
    ActionResponse,    // player's chosen action
    PlayerJoin,        // client announces itself with a name
    PlayerLeave,       // graceful disconnect

    // Bidirectional
    Ping,              // keepalive
    Pong,
    Error,
};

struct Message {
    MessageType type;
    nlohmann::json payload;

    std::string serialise() const;
    static Message deserialise(const std::string& raw);
};

} // namespace poker::net
```

- [ ] Define the JSON schema for each message in a comment block above `Message`. For example:

```
ActionRequest  payload: { "playerId": 2, "timeoutMs": 30000 }
ActionResponse payload: { "type": "RAISE", "amount": 100 }
GameStateUpdate payload: { ...full GameState fields... }
```

<details>
<summary>Concepts</summary>

> **Concept: Authoritative server**
>
> Only the server runs `GameEngine`. Clients never apply game logic locally — they display what the server tells them. This prevents cheating (clients can't forge a better hand) and keeps state consistent when multiple clients are connected.
>
> The trade-off is latency: every action requires a round-trip to the server before the UI updates. For a turn-based game like poker this is completely acceptable.

> **Concept: Protocol design principles**
>
> A good protocol is:
> - **Self-describing**: each message carries its own type so the receiver knows how to parse it without out-of-band context.
> - **Versioned**: add a `"version": 1` field now, even if you never increment it. Future you will be grateful.
> - **Error-safe**: every parser must handle unexpected message types gracefully, not crash.
>
> Using JSON is slightly verbose compared to a binary protocol (protobuf, flatbuffers) but it's debuggable — you can read messages with any text tool — and performance is not a concern for a turn-based game.

> **Concept: TCP vs WebSocket**
>
> cpp-httplib v0.11+ includes a WebSocket server/client. WebSocket sits on top of HTTP/TCP and adds:
> - A handshake that works through browser-based clients and firewalls
> - Framing: messages are delivered as discrete frames, not a raw byte stream
> - Ping/pong keepalive built into the protocol
>
> For a game client written in C++ (not a browser), raw TCP is simpler. WebSocket makes sense if you eventually want a web UI or if firewalls block raw TCP but allow port 80/443. Pick one and be consistent.

**Questions — think through these before checking answers:**
1. `ActionRequest` includes a `timeoutMs` field. What should `NetworkPlayer::getAction()` do if the client does not respond within the timeout?
2. Why does `GameStateUpdate` send the *full* `GameState` snapshot rather than a diff (only changed fields)?

</details>

<details>
<summary>Answers</summary>

**Q1.** After the timeout, `NetworkPlayer::getAction()` should return a safe default action — typically `Fold`. This prevents one disconnected or slow client from blocking the entire game indefinitely. The client should also be notified and potentially marked as disconnected.

**Q2.** Full snapshots are simpler to implement and impossible to desync. With diffs, if a single message is lost or arrives out of order, the client's state diverges from the server's permanently. Since `GameState` is small (a few dozen fields, a handful of cards), the overhead of sending it in full is negligible. Diffs are an optimisation appropriate for high-frequency state (game physics, positions) — not for poker where state changes a handful of times per second at most.

</details>

---

### Task 10.2: Serialise `GameState` to JSON

`GameState` must be convertible to JSON for transmission and back again. nlohmann/json supports custom serialisers via `to_json` / `from_json` free functions.

- [ ] Add `to_json` / `from_json` overloads in `src/core/GameState.hpp` (or a companion `GameStateSerialization.hpp`):

```cpp
// nlohmann/json looks for these in the same namespace as the type
namespace poker {

inline void to_json(nlohmann::json& j, const GameState& s) {
    j["pot"]           = s.pot;
    j["street"]        = static_cast<int>(s.street);
    j["dealerButton"]  = s.dealerButton;
    // ... all fields
}

inline void from_json(const nlohmann::json& j, GameState& s) {
    j.at("pot").get_to(s.pot);
    s.street = static_cast<Street>(j.at("street").get<int>());
    // ...
}

} // namespace poker
```

- [ ] Write round-trip tests: serialise a `GameState`, deserialise it, assert every field matches.

<details>
<summary>Concepts</summary>

> **Concept: nlohmann/json ADL serialisation**
>
> nlohmann/json uses argument-dependent lookup (ADL) to find `to_json` / `from_json`. If these functions are defined in the same namespace as the type, the library finds them automatically:
>
> ```cpp
> GameState state = ...;
> nlohmann::json j = state;           // calls poker::to_json automatically
> GameState restored = j;             // calls poker::from_json automatically
> std::string wire = j.dump();        // → JSON string for transmission
> GameState back = nlohmann::json::parse(wire);  // round-trip
> ```
>
> This is non-intrusive: `GameState` does not need to `#include` nlohmann/json itself (keep the serialisation in a separate header), so `core/` stays network-dependency-free.

> **Concept: Serialising `enum class`**
>
> nlohmann/json does not automatically serialise `enum class` — it requires explicit casts:
>
> ```cpp
> j["street"] = static_cast<int>(s.street);           // serialise
> s.street = static_cast<Street>(j.at("street").get<int>());  // deserialise
> ```
>
> An alternative is `NLOHMANN_JSON_SERIALIZE_ENUM` which maps enum values to string names — more readable on the wire and resilient to reordering enum values:
>
> ```cpp
> NLOHMANN_JSON_SERIALIZE_ENUM(Street, {
>     { Street::PreFlop,  "PreFlop"  },
>     { Street::Flop,     "Flop"     },
>     // ...
> })
> ```

**Questions — think through these before checking answers:**
1. `GameState::holeCards` is a `map<PlayerId, Hand>` — each player's private hole cards. Should `to_json` include all of them? What does the server send to each client?
2. You added `#include <nlohmann/json.hpp>` to `GameState.hpp`. What layer boundary does this violate?

</details>

<details>
<summary>Answers</summary>

**Q1.** The server's internal `GameState` always has all hole cards. But `GameStateUpdate` messages sent to a specific client must only include *that client's own hole cards* — the same privacy rule as `PlayerView`. The cleanest approach: use `makePlayerView` to filter before serialising, or build a separate `ClientGameState` struct that mirrors `GameState` but only carries `myHand` (no `holeCards` map).

**Q2.** It imports a network/serialisation library into `core/`, which is supposed to have zero external dependencies. The fix: put the `to_json` / `from_json` overloads in a separate header (`src/network/GameStateSerialization.hpp`) that `#include`s both `core/GameState.hpp` and `nlohmann/json.hpp`. Core never sees nlohmann; the network layer bridges them.

</details>

---

### Task 10.3: Implement `NetworkPlayer`

`NetworkPlayer` implements `IPlayer`. From `GameEngine`'s perspective it is indistinguishable from any other player.

- [ ] Create `src/network/NetworkPlayer.hpp`:

```cpp
#pragma once
#include "players/IPlayer.hpp"
#include <functional>
#include <future>

namespace poker {

// IPlayer implementation that proxies actions over a network connection.
// sendState:   called by dealHoleCards / getAction to push state to the client
// The returned Action comes from the client response (blocking future).
class NetworkPlayer : public IPlayer {
public:
    using SendFn = std::function<void(const std::string& message)>;

    NetworkPlayer(PlayerId id, std::string name, SendFn send);

    PlayerId    getId()                                     const override;
    std::string getName()                                   const override;
    void        dealHoleCards(const Hand& cards)                  override;
    Action      getAction(const PlayerView& view)                 override;

    // Called by GameServer when the client responds
    void provideAction(Action a);

private:
    PlayerId    m_id;
    std::string m_name;
    SendFn      m_send;

    std::promise<Action> m_actionPromise;
    std::future<Action>  m_actionFuture;
};

} // namespace poker
```

- [ ] Implement `NetworkPlayer.cpp`. `getAction` should:
  1. Reset the promise/future pair
  2. Serialise the `PlayerView` and send an `ActionRequest` to the client
  3. Block on `m_actionFuture.get()` (with timeout — use `wait_for`)
  4. Return the action, or `Fold` on timeout

<details>
<summary>Concepts</summary>

> **Concept: `std::promise` / `std::future` across a network boundary**
>
> You already used `std::promise<Action>` in Phase 6 to decouple `HumanPlayer` from SFML. The same pattern applies here: `getAction()` sets up a promise, sends a request over the network, then blocks on the future. When `GameServer` receives the client's `ActionResponse`, it calls `networkPlayer.provideAction(action)` which fulfils the promise and unblocks `getAction()`.
>
> ```
> GameEngine thread           Network thread (GameServer)
> ─────────────────           ──────────────────────────
> getAction() called
>   create promise/future
>   send ActionRequest ──────────────────────────────────► client
>   future.wait_for(30s)
>                             receive ActionResponse ◄──── client
>                             call provideAction(action)
>                               promise.set_value(action)
>   future unblocks
>   return action
> ```

> **Concept: Resetting a `std::promise`**
>
> A `std::promise` can only be fulfilled once. For repeated calls to `getAction()` (once per betting turn), you must create a fresh promise/future pair each time:
>
> ```cpp
> Action NetworkPlayer::getAction(const PlayerView& view) {
>     m_actionPromise = std::promise<Action>{};          // fresh promise
>     m_actionFuture  = m_actionPromise.get_future();    // fresh future
>     // ... send request, wait ...
> }
> ```
>
> Calling `set_value` on an already-fulfilled promise throws `std::future_error`. The reset ensures each `getAction` call has a clean slate.

> **Concept: `future::wait_for` for timeouts**
>
> ```cpp
> using namespace std::chrono_literals;
> auto status = m_actionFuture.wait_for(30s);
> if (status == std::future_status::timeout) {
>     // client too slow or disconnected
>     return Action{Action::Type::Fold};
> }
> return m_actionFuture.get();
> ```
>
> This prevents one unresponsive client from halting the game indefinitely. The timeout value should come from the game config, not be hardcoded.

**Questions — think through these before checking answers:**
1. `provideAction` is called from a network-handling thread, while `getAction` is blocking on `GameEngine`'s worker thread. Is `std::promise::set_value` safe to call from a different thread than `get_future().get()`?
2. What happens if `provideAction` is called twice for the same turn (e.g. the client sends two responses due to a bug or retry)?

</details>

<details>
<summary>Answers</summary>

**Q1.** Yes — this is the intended use case for `std::promise` / `std::future`. The standard explicitly supports setting the value from one thread and getting it from another. The synchronisation is internal to the promise/future machinery.

**Q2.** The second call to `promise.set_value` throws `std::future_error` with code `promise_already_satisfied`. You should catch this in `provideAction` and log/ignore it. The first valid response wins; subsequent responses are dropped. Alternatively, check whether the future is already ready before calling `set_value` using a flag.

</details>

---

### Task 10.4: Implement `GameServer`

`GameServer` listens for connections, maps each connection to a `NetworkPlayer`, and owns the `GameEngine`.

- [ ] Create `src/network/GameServer.hpp`:

```cpp
#pragma once
#include "core/GameEngine.hpp"
#include "NetworkPlayer.hpp"
#include <httplib.h>
#include <map>
#include <thread>
#include <memory>

namespace poker {

class GameServer {
public:
    GameServer(int port, GameConfig config);

    void start();   // blocks — runs the accept loop + game loop
    void stop();

private:
    void onClientConnect(int clientId, NetworkPlayer::SendFn send);
    void onClientMessage(int clientId, const std::string& message);
    void onClientDisconnect(int clientId);

    void runGameLoop();  // drives GameEngine::tick() in a loop

    int         m_port;
    GameConfig  m_config;

    std::map<int, std::shared_ptr<NetworkPlayer>> m_players;
    std::unique_ptr<GameEngine>                   m_engine;  // created once all players join

    httplib::Server m_server;  // or a WebSocket server
    std::thread     m_gameThread;
};

} // namespace poker
```

- [ ] Implement `GameServer.cpp`. The lifecycle is:
  1. Wait for `config.numPlayers` clients to send `PlayerJoin`
  2. Create `NetworkPlayer` instances and construct `GameEngine`
  3. Start `runGameLoop()` on a thread — calls `engine.tick()` in a loop
  4. Route incoming `ActionResponse` messages to the correct `NetworkPlayer::provideAction`
  5. After each `tick()`, broadcast `GameStateUpdate` to all clients

<details>
<summary>Concepts</summary>

> **Concept: Lobby phase**
>
> The game can't start until all seats are filled. The server holds a "lobby" state between startup and the first `engine.tick()`. During the lobby:
> - Clients connect and send `PlayerJoin` with their display name
> - Server acknowledges and assigns a `PlayerId`
> - Once enough players join, the server creates `GameEngine` and starts the game loop
>
> A simple state machine for the server: `Lobby → Playing → GameOver`.

> **Concept: Broadcasting vs unicasting**
>
> After each `GameEngine::tick()`, the server must send a `GameStateUpdate` to every client. Each client gets a *filtered* view (only their own hole cards). This is broadcasting with per-recipient filtering — not the same as sending an identical message to everyone.
>
> ```cpp
> for (auto& [id, player] : m_players) {
>     PlayerView view = makePlayerView(engine.getStateSnapshot(), id);
>     player->send(serialise(MessageType::GameStateUpdate, view));
> }
> ```

> **Concept: Thread safety in GameServer**
>
> Two threads touch shared state:
> - The **game thread** calls `engine.tick()` and reads `m_players`
> - The **network thread** (httplib's accept loop) calls `onClientMessage` and writes to `NetworkPlayer`'s promise
>
> `NetworkPlayer::provideAction` is the only shared mutation — and it's protected by the promise/future mechanism itself. If you add any shared collections (e.g. a message queue), protect them with `std::mutex`.

**Questions — think through these before checking answers:**
1. A client disconnects mid-hand. `NetworkPlayer::getAction` is blocking on `m_actionFuture`. What should `onClientDisconnect` do to unblock it?
2. `runGameLoop` calls `engine.tick()` in a tight loop. What should it do between ticks to avoid spinning the CPU at 100%?

</details>

<details>
<summary>Answers</summary>

**Q1.** Call `networkPlayer.provideAction(Action{Action::Type::Fold})` and mark the player as disconnected. This fulfils the promise with a fold, letting the game continue. Future calls to `getAction` on the disconnected player should immediately return `Fold` without sending a network request — check a `m_connected` flag at the top of `getAction`.

**Q2.** `GameEngine::tick()` already blocks internally — `runBettingRound()` blocks on each `player->getAction()`, which for `NetworkPlayer` blocks until the client responds. So the game loop is naturally throttled to the pace of player input. No explicit sleep is needed. If you later add a mode with all-AI players (no `NetworkPlayer`), AI `getAction` calls return quickly and you may want a small sleep to keep the game at a human-watchable pace.

</details>

---

### Task 10.5: Implement `GameClient`

`GameClient` connects to the server, receives state updates, drives the local renderer, and forwards human player actions.

- [ ] Create `src/network/GameClient.hpp`:

```cpp
#pragma once
#include "core/PlayerView.hpp"
#include "core/Types.hpp"
#include <functional>
#include <string>
#include <thread>

namespace poker {

class GameClient {
public:
    using OnStateUpdate = std::function<void(const PlayerView&)>;
    using OnActionRequest = std::function<void()>;

    GameClient(std::string host, int port, std::string playerName);

    void connect();
    void disconnect();

    // Set before calling connect()
    void setOnStateUpdate(OnStateUpdate cb)   { m_onStateUpdate = cb; }
    void setOnActionRequest(OnActionRequest cb) { m_onActionRequest = cb; }

    // Called by InputHandler when the human picks an action
    void sendAction(Action a);

private:
    void receiveLoop();
    void handleMessage(const std::string& raw);

    std::string       m_host;
    int               m_port;
    std::string       m_playerName;
    PlayerId          m_myId = -1;

    OnStateUpdate    m_onStateUpdate;
    OnActionRequest  m_onActionRequest;

    std::thread      m_receiveThread;
    // connection handle (httplib or raw socket)
};

} // namespace poker
```

- [ ] Update `main.cpp` to support a `--server` and `--client HOST:PORT` flag:

```cpp
// main.cpp (pseudocode)
if (args has "--server") {
    GameServer server(port, config);
    server.start();  // blocking
} else if (args has "--client HOST:PORT") {
    GameClient client(host, port, "Player");
    client.setOnStateUpdate([&](const PlayerView& v) { renderer.update(v); });
    client.setOnActionRequest([&]() { renderer.showActionButtons(); });
    client.connect();
    sfmlLoop();  // existing SFML main loop — renderer reads from client
}
```

<details>
<summary>Concepts</summary>

> **Concept: Callbacks vs polling for network events**
>
> `GameClient` receives messages asynchronously on a background thread. Two patterns for delivering those messages to the main (SFML) thread:
>
> **Callbacks (chosen here):** The receive thread calls `m_onStateUpdate(view)` directly. Simple, but the callback runs on the network thread — if it touches SFML objects, you need synchronisation.
>
> **Message queue:** The receive thread pushes messages into a `std::queue` protected by a `std::mutex`. The SFML main thread drains the queue each frame with `pollMessages()`. Slightly more boilerplate but keeps all SFML operations on the main thread, which SFML requires.
>
> The message queue approach is safer and is the pattern used in Phase 8's renderer.

> **Concept: Command-line argument parsing**
>
> C++ has no standard argument parsing library. For simple flags, a manual loop over `argv` is fine:
>
> ```cpp
> int main(int argc, char* argv[]) {
>     for (int i = 1; i < argc; ++i) {
>         std::string arg = argv[i];
>         if (arg == "--server") { /* ... */ }
>         else if (arg == "--client" && i + 1 < argc) {
>             std::string addr = argv[++i];  // "localhost:9999"
>             // split on ':'
>         }
>     }
> }
> ```
>
> For more complex CLIs, consider a lightweight library. The project already has no dependency for this, so a manual loop is appropriate.

**Questions — think through these before checking answers:**
1. `GameClient::receiveLoop` runs on a background thread. SFML requires all rendering to happen on the main thread. What is the correct way to hand off a received `PlayerView` to the renderer?
2. If the server restarts (crashes and comes back), can the client reconnect without restarting the game application? What state would need to be reset?

</details>

<details>
<summary>Answers</summary>

**Q1.** Use a `std::mutex`-protected `std::queue<PlayerView>` shared between the receive thread and the main thread. The receive thread pushes incoming views; the SFML loop (on the main thread) calls `client.pollUpdates()` each frame and renders any queued views. This is the same pattern as Phase 8's `GameEngine` → renderer state handoff.

**Q2.** Reconnection is non-trivial. The client would need to re-send `PlayerJoin`, receive the current `GameStateUpdate` to rebuild its view, and re-enter the game in progress. If the server assigns `PlayerId` based on join order, the reconnecting client must get the same ID back. In practice, reconnection requires a session token (`UUID` generated on first connect, reused on reconnect) so the server can map the new connection to the existing `NetworkPlayer` slot.

</details>

---

### Task 10.6: Handle disconnections and test locally

- [ ] Implement the disconnection path in `GameServer::onClientDisconnect`:
  - Mark the player as disconnected
  - Call `provideAction(Fold)` if they were waiting to act
  - Broadcast a `PlayerLeft` message to remaining clients
  - Decide policy: remove the player from the game, or keep their seat empty (they fold every hand)

- [ ] Test the full flow locally using two processes on the same machine:

```bash
# Terminal 1 — start server
./build/texas-holdem --server --port 9999 --players 2

# Terminal 2 — connect client 1
./build/texas-holdem --client localhost:9999 --name Alice

# Terminal 3 — connect client 2
./build/texas-holdem --client localhost:9999 --name Bob
```

- [ ] Verify chip conservation across hands by logging server-side totals after each hand.

- [ ] Commit:

```bash
git add src/network/ tests/network/
git commit -m "feat(network): add GameServer, GameClient, NetworkPlayer for online multiplayer"
```

---

## Evaluation

### Build checklist

- [ ] `cmake --build` exits with code `0`
- [ ] `ctest` passes — all existing tests still pass (networking must not break core logic)
- [ ] Two processes can play a hand to completion without hanging or crashing
- [ ] Disconnecting a client mid-hand causes a fold (not a freeze)
- [ ] Chip conservation holds across multiple hands (server-side assert or log check)

### Concept checklist

- [ ] Does `NetworkPlayer` implement `IPlayer` — does `GameEngine` have any awareness it's talking over a network?
- [ ] Are hole cards filtered before being sent to each client (no opponent cards leaked)?
- [ ] Is `GameState` serialisation kept out of `src/core/` (in `src/network/` instead)?
- [ ] Is the `std::promise` reset before each `getAction` call?
- [ ] Does `GameServer` send per-client filtered `PlayerView` snapshots, not the raw `GameState`?

### Common mistakes

| Mistake | Symptom | Fix |
|---|---|---|
| Sending raw `GameState` to all clients | Each client can see all hole cards | Use `makePlayerView` per client before serialising |
| Forgetting to reset `std::promise` between turns | Second `getAction` call throws `std::future_error` | Assign a fresh `std::promise<Action>{}` at the top of each `getAction` |
| SFML calls on the receive thread | Crash or graphical corruption | Route state updates through a `std::queue`; drain it on the main SFML thread |
| Tight loop in `runGameLoop` with AI-only players | 100% CPU usage | Add a short `std::this_thread::sleep_for(100ms)` between AI-only ticks |
| No disconnection handling | Game freezes if any client closes the window | Call `provideAction(Fold)` in `onClientDisconnect` |
