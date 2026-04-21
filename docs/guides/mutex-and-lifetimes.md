# Mutexes and Variable Lifetimes in C++17

Two concepts that underpin the threading model in this project:

1. **Mutexes** — prevent data races when two threads access shared state
2. **Variable lifetimes** — govern when objects are created and destroyed

Both are easier to reason about through the RAII pattern C++ uses for almost everything.

---

## Part 1 — Mutexes

### Why you need one

A **data race** occurs when two threads access the same memory at the same time and at
least one access is a write. The result is undefined behaviour — crashes, silently wrong
values, or demons flying out of your nose.

```
Thread A (engine)          Thread B (renderer)
──────────────────         ──────────────────
m_state.pot = 300;         read m_state.pot   ← could see 0, 300, or garbage
```

A mutex (mutual exclusion lock) makes only one thread at a time enter the protected region:

```
Thread A (engine)          Thread B (renderer)
──────────────────         ──────────────────
lock mutex                 try to lock mutex → blocks
m_state.pot = 300;         (waiting…)
unlock mutex               lock acquired
                           read m_state.pot   ← guaranteed to see 300
                           unlock mutex
```

---

### `std::mutex` — the primitive

```cpp
#include <mutex>

std::mutex m;

// Thread A
m.lock();
shared_data = 42;
m.unlock();   // easy to forget, and exception-unsafe!

// Thread B
m.lock();
int val = shared_data;
m.unlock();
```

**Don't use `lock()`/`unlock()` directly.** If an exception is thrown between lock and
unlock, the mutex is never released and every other thread blocks forever (deadlock).

---

### `std::lock_guard` — RAII lock (preferred)

```cpp
#include <mutex>

std::mutex m;

{
    std::lock_guard<std::mutex> guard(m);  // locks on construction
    shared_data = 42;
}                                          // destructor runs → unlocks automatically
                                           // even if an exception is thrown
```

The lock is tied to the *lifetime of the guard object*. When the guard goes out of scope
(end of block, early return, exception unwinding — anything), the destructor runs and
the mutex is released. You cannot forget to unlock.

This is the RAII pattern: **Resource Acquisition Is Initialisation**. The resource (lock)
is acquired in the constructor and released in the destructor.

**In this project:**

```cpp
// GameEngine.cpp
GameState GameEngine::getStateSnapshot() {
    std::lock_guard<std::mutex> lock(m_stateMutex);  // lock acquired here
    return m_state;                                   // copy made while holding lock
}                                                     // lock released here — safe even
                                                      // if copy constructor throws
```

---

### `std::unique_lock` — flexible lock

`std::unique_lock` is a movable, manually-controllable RAII lock. It has the same
safety guarantee as `lock_guard` (unlocks on destruction) but adds several capabilities
`lock_guard` deliberately omits.

#### Example 1 — early unlock, then re-lock in the same scope

Sometimes you need to release the lock mid-function to call something slow, then
re-acquire it to update shared state — without opening a new nested block:

```cpp
void processAction() {
    std::unique_lock<std::mutex> lock(m_stateMutex);

    // snapshot state while holding the lock
    ActionContext ctx = buildContext(m_state);

    lock.unlock();           // release — expensive work below doesn't block other threads

    Result result = doExpensiveWork(ctx);  // could take milliseconds or more

    lock.lock();             // re-acquire before writing back
    applyResult(m_state, result);

}   // destructor unlocks (lock is held here)
```

Compare this to the `lock_guard` approach in `runBettingRound()`, which uses *two separate
scopes* with two separate guards to achieve the same pattern. Both are correct; `unique_lock`
avoids the nested block when the split happens in the middle of a function's logic flow.

---

#### Example 2 — deferred locking

You can construct a `unique_lock` *without* locking immediately, then lock at the right moment:

```cpp
std::unique_lock<std::mutex> lock(m, std::defer_lock);  // NOT locked yet

// do setup that doesn't need the lock
prepareData();

lock.lock();   // now lock, exactly where needed
writeSharedState();

// lock released at end of scope as usual
```

This is useful when the lock acquisition needs to happen conditionally, or when you want
to construct the guard object early for RAII safety but delay the actual lock.

---

#### Example 3 — `try_lock` (non-blocking attempt)

`unique_lock` can attempt to acquire the lock without blocking:

```cpp
std::unique_lock<std::mutex> lock(m, std::try_to_lock);

if (lock.owns_lock()) {
    // got the lock — update state
    m_state.pot += amount;
} else {
    // couldn't acquire — do something else or skip
    queueForLater(amount);
}
```

Useful in scenarios where a thread should do other work rather than block indefinitely —
e.g., a renderer that falls back to the previous frame if the engine thread holds the lock.

---

#### Example 4 — condition variables (requires `unique_lock`)

`std::condition_variable::wait()` releases the mutex and sleeps until notified, then
re-acquires before returning. It **requires** `unique_lock` because it needs to release
and re-acquire mid-scope — something `lock_guard` cannot do.

```cpp
std::mutex m;
std::condition_variable cv;
bool ready = false;

// Thread A — producer
{
    std::lock_guard<std::mutex> lock(m);
    ready = true;
}
cv.notify_one();

// Thread B — consumer
{
    std::unique_lock<std::mutex> lock(m);       // unique_lock required here
    cv.wait(lock, []{ return ready; });          // atomically: release lock, sleep,
                                                 // re-acquire when notified + ready==true
    // lock is held again here — safe to read
    consumeData();
}
```

`cv.wait(lock, predicate)` is equivalent to:
```cpp
while (!predicate()) {
    cv.wait(lock);   // releases lock, sleeps, re-acquires on wake
}
```
The predicate guards against **spurious wakeups** — the OS can occasionally wake a waiting
thread even when `notify_one()` wasn't called. Without it you'd read stale state.

---

#### Example 5 — transferring lock ownership

`unique_lock` is movable, so you can return it from a function or pass it between scopes:

```cpp
std::unique_lock<std::mutex> acquireWhenReady(std::mutex& m) {
    std::unique_lock<std::mutex> lock(m);
    // ... wait for preconditions ...
    return lock;   // ownership of the lock transfers to the caller
                   // mutex stays locked across the return
}

void doWork() {
    auto lock = acquireWhenReady(m_stateMutex);
    // mutex is locked here — safe to read/write
    m_state.pot = 100;
}   // lock destroyed here → mutex released
```

`lock_guard` is not movable and cannot do this. This pattern is rare but handy when
lock acquisition logic belongs in a helper function.

---

#### When to use which

| Situation | Use |
|---|---|
| Simple critical section, one lock | `lock_guard` |
| Need to unlock early / re-lock later | `unique_lock` |
| Condition variable | `unique_lock` (required) |
| Non-blocking try-lock | `unique_lock` with `try_to_lock` |
| Deferred lock acquisition | `unique_lock` with `defer_lock` |
| Multiple mutexes at once (deadlock-safe) | `std::scoped_lock` (C++17) |
| Transferring lock ownership | `unique_lock` (movable) |

The rule of thumb: **start with `lock_guard`**. Reach for `unique_lock` only when you
need one of the capabilities above.

---

### `mutable` — locking in `const` methods

A `const` method promises not to modify the object. But *locking a mutex* is a write
operation on the mutex itself. The `mutable` keyword marks a member as modifiable even
through a `const` reference:

```cpp
class GameEngine {
    mutable std::mutex m_stateMutex;   // mutable: can be locked in const methods
    GameState m_state;

public:
    GameState getStateSnapshot() const {          // const — promises not to change game state
        std::lock_guard<std::mutex> lock(m_stateMutex);  // OK because mutex is mutable
        return m_state;
    }
};
```

Without `mutable`, the compiler would reject `lock_guard<std::mutex>(m_stateMutex)` inside
a `const` method because constructing the guard modifies the mutex.

---

### Granular vs coarse locking

**Coarse lock** — hold the lock for the entire operation:

```cpp
void GameEngine::tick() {
    std::lock_guard<std::mutex> lock(m_stateMutex);  // locked for the whole betting round
    runBettingRound();   // calls player->getAction() which can block for SECONDS
}                        // renderer is completely frozen the whole time
```

**Granular lock** — lock only around shared-state reads/writes, release around blocking calls:

```cpp
void GameEngine::runBettingRound() {
    while (!needsToAct.empty()) {

        PlayerView view;
        IPlayer* player;
        {
            std::lock_guard<std::mutex> lock(m_stateMutex);  // locked briefly
            m_state.activePlayer     = activeId;
            m_state.waitingForAction = true;
            view   = makePlayerView(m_state, activeId);      // snapshot under lock
            player = findPlayer(activeId);
        }                                                      // lock released here

        Action action = player->getAction(view);              // may block for seconds
                                                              // renderer can read freely

        {
            std::lock_guard<std::mutex> lock(m_stateMutex);  // lock again to apply result
            m_state.waitingForAction = false;
            // ... apply action ...
        }                                                      // lock released
    }
}
```

This is why `tick()` has *no* top-level lock — it deliberately releases the mutex around
every call to `getAction()` so the renderer thread gets to read state between each player
decision.

---

### Common pitfalls

| Pitfall | What happens | Fix |
|---|---|---|
| Forgetting to unlock | Deadlock on next lock() call | Use `lock_guard` |
| Holding lock across blocking I/O | Renders/UI freezes | Release lock before blocking call |
| Locking in constructor | Lock isn't needed; no other thread has `this` yet | Don't lock in constructors |
| Returning a reference to locked data | Reference outlives the lock | Return by value (copy) |
| Two mutexes, locked in opposite order | Classic deadlock | Always lock in same order, or use `std::scoped_lock` |
| Recursive lock on `std::mutex` | Immediate deadlock | Use `std::recursive_mutex` if truly needed |

---

## Part 2 — Variable Lifetimes

### Stack vs heap

Every variable has a *storage duration* that determines when it's created and destroyed.

**Stack (automatic storage):**

```cpp
void foo() {
    int x = 5;          // created on entry to foo()
    Card c(Suit::Hearts, Rank::Ace);  // constructor runs here
    // ... use c ...
}                       // x and c destroyed here — destructor runs automatically
```

Stack objects are destroyed in *reverse* order of construction when the enclosing scope
ends. This is guaranteed and deterministic. No heap allocation, no manual cleanup.

**Heap (dynamic storage):**

```cpp
Card* c = new Card(Suit::Hearts, Rank::Ace);  // allocated on heap
// ... use c ...
delete c;  // must call manually; if you forget → memory leak
```

Heap is needed when:
- The lifetime must outlast the creating scope
- The size isn't known at compile time (e.g., `std::vector`)
- Ownership needs to transfer between scopes

---

### RAII — the unifying principle

The key insight of modern C++ is: **tie the lifetime of a heap resource to a stack object**.
When the stack object is destroyed, its destructor cleans up the heap resource.

```cpp
{
    std::unique_ptr<Card> c = std::make_unique<Card>(Suit::Hearts, Rank::Ace);
    // heap memory allocated, managed by c (stack object)
    // ... use *c ...
}   // c destroyed → destructor calls delete on the Card automatically
    // no leak, even if an exception is thrown inside the block
```

This is RAII in action: `unique_ptr` is just a stack wrapper around a heap pointer.
`std::vector`, `std::string`, `std::lock_guard` — all RAII wrappers.

---

### Object lifetimes and move semantics

When you `std::move` a `unique_ptr`, ownership transfers:

```cpp
auto p1 = std::make_unique<Card>(Suit::Hearts, Rank::Ace);  // p1 owns the Card
auto p2 = std::move(p1);   // p2 now owns the Card; p1 is null
// *p2 is valid; *p1 is undefined behaviour
```

This is used throughout `GameEngine`:

```cpp
GameEngine::GameEngine(std::vector<std::unique_ptr<IPlayer>> players, GameConfig config)
    : m_players(std::move(players))  // ownership moves from caller into m_players
```

After this constructor runs, the caller's `players` vector is empty (all ptrs moved out).
`m_players` owns all the `IPlayer` objects for the lifetime of `GameEngine`.

---

### Dangling references — the most common lifetime bug

A **dangling reference** (or pointer) refers to an object that has already been destroyed:

```cpp
// BAD: returning a reference to a local
const std::string& bad() {
    std::string s = "hello";  // stack variable
    return s;                 // s is destroyed when bad() returns — caller gets dangling ref
}

// BAD: storing a pointer to a temporary
int* p;
{
    int x = 42;
    p = &x;    // x lives here
}              // x destroyed
*p = 99;       // undefined behaviour — x is gone
```

**In the mutex context**, this is why `getStateSnapshot()` returns by *value*, not by reference:

```cpp
// WRONG — data race after lock releases
const GameState& GameEngine::getStateSnapshot() {
    std::lock_guard<std::mutex> lock(m_stateMutex);
    return m_state;   // lock releases on return; caller reads m_state without lock
}

// CORRECT — copy made under lock; caller owns the copy
GameState GameEngine::getStateSnapshot() {
    std::lock_guard<std::mutex> lock(m_stateMutex);
    return m_state;   // copy constructed here while lock is held
}                     // lock releases; caller has a safe, independent copy
```

---

### Lifetime of lambda captures

Lambdas can capture by value or by reference. Capturing by reference is a common
source of dangling bugs:

```cpp
// DANGEROUS: lambda outlives the captured variable
std::function<void()> make_lambda() {
    int x = 42;
    return [&x]() { std::cout << x; };  // x is on the stack of make_lambda()
}                                        // x destroyed here

auto f = make_lambda();
f();   // undefined behaviour — x is gone
```

**Fix:** capture by value when the lambda might outlive the scope:

```cpp
std::function<void()> make_lambda() {
    int x = 42;
    return [x]() { std::cout << x; };  // x copied into the lambda
}
auto f = make_lambda();
f();   // safe — lambda owns its own copy of x
```

In this project, the `deduct` lambda in `postBlinds()` captures `this` by reference
(`[&]`) — safe because the lambda doesn't outlive the method call.

---

### Summary table

| Concept | Lifetime | Cleanup |
|---|---|---|
| Stack variable | Scope it was declared in | Automatic (destructor) |
| `std::unique_ptr` | Until moved or enclosing scope ends | Automatic (`delete` in destructor) |
| `std::shared_ptr` | Until last copy goes out of scope | Automatic (ref-counted `delete`) |
| `std::lock_guard` | Scope it was declared in | Automatic (unlocks mutex) |
| Raw `new` | Until explicit `delete` | Manual — use RAII wrappers instead |
| Lambda `[&]` capture | Borrowed — must not outlive source | Borrowed reference — you own nothing |
| Lambda `[=]` capture | Owned copy inside the lambda | Automatic when lambda is destroyed |
