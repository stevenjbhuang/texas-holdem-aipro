# Interfaces in C++17

C++ has no `interface` keyword. Instead, interfaces are expressed as **abstract classes** —
classes with only pure virtual functions.

---

## Defining an interface

```cpp
class IPlayer {
public:
    virtual ~IPlayer() = default;                      // always required (see below)
    virtual Action getAction(const GameState&) = 0;    // pure virtual — must be implemented
    virtual std::string getName() const = 0;
};
```

- `= 0` makes a function **pure virtual** — the class becomes abstract
- Abstract classes cannot be instantiated directly
- Any class that doesn't implement all pure virtual functions is also abstract

```cpp
IPlayer p;  // COMPILE ERROR — cannot instantiate abstract class
```

---

## Implementing an interface

```cpp
class HumanPlayer : public IPlayer {
public:
    explicit HumanPlayer(const std::string& name) : m_name(name) {}

    Action getAction(const GameState& state) override { ... }
    std::string getName() const override { return m_name; }

private:
    std::string m_name;
};
```

- `: public IPlayer` — HumanPlayer inherits from IPlayer
- `override` — tells the compiler "this is intentionally overriding a virtual function"
  (compiler error if you mistype the name or signature)

---

## The virtual destructor rule

**Always declare `virtual ~IBase() = default;` on every interface.** Without it,
deleting through a base pointer skips the derived class destructor:

```cpp
// Without virtual destructor:
IPlayer* player = new HumanPlayer("Alice");
delete player;  // calls ~IPlayer() only — ~HumanPlayer() never runs → resource leak

// With virtual destructor:
IPlayer* player = new HumanPlayer("Alice");
delete player;  // calls ~HumanPlayer() first, then ~IPlayer() — correct
```

`= default` means "compiler-generated destructor is fine, just make it virtual."

---

## Why interfaces?

### Decoupling

`GameEngine` doesn't need to know whether a player is human or AI:

```cpp
// Without interface — GameEngine is coupled to concrete types:
class GameEngine {
    HumanPlayer* m_human;
    AIPlayer* m_ai;
    // Adding a new player type requires changing GameEngine
};

// With interface — GameEngine is decoupled:
class GameEngine {
    std::vector<std::unique_ptr<IPlayer>> m_players;
    // Add any new player type without touching GameEngine
};
```

### Testability

Swap in a fake implementation during tests:

```cpp
class MockLLMClient : public ILLMClient {
public:
    std::string sendPrompt(const std::string&, const std::string& = "") override {
        return "fold";  // always returns fold — predictable for testing
    }
};

// Inject the mock instead of OllamaClient:
auto ai = std::make_unique<AIPlayer>(std::make_unique<MockLLMClient>());
```

No real network calls in tests. No Ollama server needed.

---

## Virtual dispatch

When you call a method through an interface pointer, C++ looks up the **actual type**
at runtime and calls the right implementation:

```cpp
std::vector<std::unique_ptr<IPlayer>> players;
players.push_back(std::make_unique<HumanPlayer>("Alice"));
players.push_back(std::make_unique<AIPlayer>("Bot1"));

for (auto& player : players) {
    player->getAction(state);
    // Alice  → HumanPlayer::getAction() — waits for user input
    // Bot1   → AIPlayer::getAction()   — calls LLM
}
```

This is called **polymorphism** — one interface, many implementations.

---

## Interfaces in this project

| Interface | Implementations | Used by |
|-----------|----------------|---------|
| `IPlayer` | `HumanPlayer`, `AIPlayer` | `GameEngine` |
| `ILLMClient` | `OpenAICompatibleClient`, `MockLLMClient` | `AIPlayer` |

Each layer only knows the interface — never the concrete type. This is the
core of the decoupled architecture.

---

## Quick reference

| Concept | Syntax |
|---------|--------|
| Pure virtual function | `virtual void foo() = 0;` |
| Virtual destructor | `virtual ~IBase() = default;` |
| Implementing an interface | `: public IBase` + `override` on each method |
| Storing interface objects | `std::unique_ptr<IBase>` (see [pointers guide](pointers.md)) |

**Rule of thumb:** prefix interface names with `I` (`IPlayer`, `ILLMClient`) so it's
immediately clear a type is an interface, not a concrete class.
