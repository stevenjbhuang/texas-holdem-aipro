# Pointers in C++17

C++ has several kinds of pointers. Knowing which to use — and when — is one of the
most important skills in modern C++.

---

## Raw pointers (avoid)

A raw pointer is just an address in memory. You manage the memory yourself.

```cpp
Card* card = new Card(Suit::Hearts, Rank::Ace);  // allocate
// ... use card ...
delete card;  // free — easy to forget
card = nullptr;  // good habit, but easy to forget too
```

**Problems:**
- Forget `delete` → memory leak
- `delete` twice → undefined behaviour (crash)
- Exception thrown before `delete` → memory leak
- No clear indication of who owns the pointer

**When to use:** almost never for ownership. Raw pointers are still fine as
*non-owning observers* (see below).

---

## `std::unique_ptr` — single owner

`unique_ptr` owns the object. When it goes out of scope, the object is deleted automatically.

```cpp
#include <memory>

auto card = std::make_unique<Card>(Suit::Hearts, Rank::Ace);
// ... use card ...
// no delete needed — freed automatically when card goes out of scope
```

**One owner at a time — cannot copy, can move:**

```cpp
auto a = std::make_unique<Card>(Suit::Hearts, Rank::Ace);
auto b = a;             // COMPILE ERROR — copying not allowed
auto b = std::move(a); // OK — ownership transfers to b, a becomes null
```

**Storing polymorphic types:**

```cpp
std::vector<std::unique_ptr<IPlayer>> players;
players.push_back(std::make_unique<HumanPlayer>("Alice"));
players.push_back(std::make_unique<AIPlayer>("Bot1"));

for (auto& player : players)
    player->getAction(state);  // virtual dispatch — calls the right implementation
```

**In this project:**
```cpp
// GameEngine owns the players
std::vector<std::unique_ptr<IPlayer>> m_players;

// AIPlayer owns its LLM client
std::unique_ptr<ILLMClient> m_client;
```

**Rule of thumb:** reach for `unique_ptr` any time you'd write `new`. It's the default
smart pointer.

---

## `std::shared_ptr` — shared ownership

`shared_ptr` allows **multiple owners**. It keeps a reference count — the object is
deleted when the last `shared_ptr` to it is destroyed.

```cpp
#include <memory>

auto a = std::make_shared<Card>(Suit::Hearts, Rank::Ace);
auto b = a;  // OK — both a and b own the card, ref count = 2

// a goes out of scope → ref count = 1, card still alive
// b goes out of scope → ref count = 0, card deleted
```

**When to use:** when ownership is genuinely shared between multiple things and you
can't determine a single clear owner.

```cpp
// Example: a texture loaded once, shared across many sprites
auto texture = std::make_shared<sf::Texture>();
texture->loadFromFile("card.png");

Sprite spriteA(texture);  // both hold a shared_ptr to the same texture
Sprite spriteB(texture);
```

**Avoid overusing it.** `shared_ptr` has runtime overhead (atomic ref count) and can
hide unclear ownership design. If you find yourself reaching for `shared_ptr` often,
it's usually a sign the ownership model needs rethinking.

---

## `std::weak_ptr` — non-owning observer of a `shared_ptr`

`weak_ptr` holds a reference to a `shared_ptr`-managed object **without keeping it alive**.
You must lock it before use to check if the object still exists.

```cpp
std::shared_ptr<Card> card = std::make_shared<Card>(Suit::Hearts, Rank::Ace);
std::weak_ptr<Card> observer = card;  // does not increment ref count

// Later — check if the object is still alive before using it:
if (auto locked = observer.lock()) {
    // locked is a shared_ptr — safe to use
    locked->getRank();
} else {
    // object has been deleted
}
```

**When to use:** breaking circular references between `shared_ptr`s, or when something
needs to observe an object without owning it.

```cpp
// Circular reference problem:
struct A { std::shared_ptr<B> b; };
struct B { std::shared_ptr<A> a; };  // A keeps B alive, B keeps A alive — never deleted!

// Fix: break the cycle with weak_ptr
struct B { std::weak_ptr<A> a; };    // B observes A without owning it
```

---

## Raw pointers as non-owning observers

Raw pointers are fine when you just want to *look at* something you don't own.
The key is clarity: raw pointer = no ownership, no deletion.

```cpp
void printCard(const Card* card) {
    // We're just observing — we don't own card, we don't delete it
    std::cout << card->toString();
}

auto card = std::make_unique<Card>(Suit::Hearts, Rank::Ace);
printCard(card.get());  // .get() returns the raw pointer without transferring ownership
```

Alternatively, use a **reference** (`const Card&`) when the pointer will never be null —
references are cleaner for non-owning observation:

```cpp
void printCard(const Card& card) {
    std::cout << card.toString();
}

printCard(*card);  // dereference the unique_ptr
```

---

## Summary

| Pointer type | Ownership | Use when |
|---|---|---|
| `T*` (raw) | None | Non-owning observer, interfacing with C APIs |
| `std::unique_ptr<T>` | Single owner | Default choice — one clear owner |
| `std::shared_ptr<T>` | Shared | Genuinely shared ownership (use sparingly) |
| `std::weak_ptr<T>` | None | Observing a `shared_ptr` without owning it |
| `T&` (reference) | None | Non-owning, never null — prefer over raw `T*` |

**Decision order:**
1. Can you use a reference? Use `T&` or `const T&`.
2. Is there one clear owner? Use `unique_ptr`.
3. Is ownership genuinely shared? Use `shared_ptr`.
4. Do you need to observe a `shared_ptr` without owning it? Use `weak_ptr`.
5. Interfacing with a C API or legacy code? Use raw `T*` carefully.

See [interfaces.md](interfaces.md) for how `unique_ptr` pairs with interfaces and
virtual dispatch.
