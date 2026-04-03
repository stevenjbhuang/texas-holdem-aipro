# Phase 7 — UI Layer: SFML Setup, Renderer, Input

> **Status: COMPLETE** (commit `f64bf83`) — SetupScreen, GameRenderer, InputHandler implemented. SFML window opens, cards render, action buttons work.

**Concepts you'll learn:** Game loop pattern, SFML window and drawing primitives, `sf::Font` lifetime, thread-safe state snapshots, hit-testing for buttons.

**Previous phase:** [Phase 6 — Players Layer](phase-6-players-layer.md)
**Next phase:** [Phase 8 — Wire It All Together](phase-8-wire-up.md)

**Design reminder:** All `sf::RenderWindow` and `sf::Event` calls must happen on the **main thread** — the thread that created the window. `GameEngine::tick()` runs on a worker thread and never touches SFML. The UI reads state via `engine.getStateSnapshot()`, which returns a mutex-protected copy.

```
Main thread                          Worker thread
───────────────────────────          ─────────────────────
while (window.isOpen()) {
  pollEvent → InputHandler           GameEngine::tick()
  snapshot = engine.getStateSnapshot()   (drives game state)
  renderer.render(snapshot)
  inputHandler.drawButtons(snapshot)
  window.display()
}
```

---

### Task 7.1: Implement `SetupScreen`

`SetupScreen` runs before the game starts. It lets the user configure the game — number of AI players, starting stack, blind sizes — and returns a `GameConfig` when the user clicks Start. Implementing it first gives you a working SFML window and event loop to build on before the game logic is connected.

- [ ] Create `src/ui/SetupScreen.hpp`:

```cpp
#pragma once
#include "core/GameEngine.hpp"
#include <SFML/Graphics.hpp>

namespace poker {

class SetupScreen {
public:
    SetupScreen(sf::RenderWindow& window, sf::Font& font);

    // Blocks and processes events until the user clicks Start.
    // Returns the configured GameConfig.
    GameConfig run();

private:
    void draw();

    sf::RenderWindow& m_window;
    sf::Font&         m_font;

    int m_numAIPlayers  = 3;
    int m_startingStack = 1000;
    int m_smallBlind    = 5;
    int m_bigBlind      = 10;
};

} // namespace poker
```

- [ ] Implement `src/ui/SetupScreen.cpp`. **Your turn.** Key points:
  - `run()` contains a standard SFML event loop: `pollEvent` → handle → `draw()` → `window.display()`
  - Draw each setting as a label and a value with `+`/`-` buttons on either side using `sf::RectangleShape` and `sf::Text`
  - Hit-test mouse clicks with `sf::FloatRect::contains(event.mouseButton.x, event.mouseButton.y)`
  - When the user clicks Start, construct and return `GameConfig(m_numAIPlayers + 1, m_startingStack, m_smallBlind, m_bigBlind)`

<details>
<summary>Concepts</summary>

> **Concept: The SFML game loop**
>
> SFML's core loop has three steps that repeat every frame:
>
> ```cpp
> while (window.isOpen()) {
>     // 1. Poll events — keyboard, mouse, window close
>     sf::Event event;
>     while (window.pollEvent(event)) {
>         if (event.type == sf::Event::Closed)
>             window.close();
>         // handle other events...
>     }
>
>     // 2. Clear, draw, display
>     window.clear(sf::Color(0, 100, 0));  // dark green
>     window.draw(someShape);
>     window.display();                    // swap front/back buffer
> }
> ```
>
> `pollEvent` is non-blocking — it processes one queued event per call and returns `false` when the queue is empty. The inner `while` loop drains the entire queue each frame. `window.display()` swaps the back buffer to the screen; without it, nothing is ever visible.

> **Concept: `sf::Text` and `sf::Font` — font lifetime**
>
> `sf::Text` stores a *pointer* to the `sf::Font` it was constructed with. If the `sf::Font` is destroyed before the `sf::Text`, the text renders as blank rectangles or crashes:
>
> ```cpp
> // ❌ Dangling pointer — font destroyed when function returns:
> void drawLabel(sf::RenderWindow& w, const std::string& str) {
>     sf::Font font;
>     font.loadFromFile("arial.ttf");
>     sf::Text text(str, font, 24);  // text holds a pointer to font
>     w.draw(text);
> }  // ← font destroyed here; text is already dead
>
> // ✓ Font outlives text — store both as members:
> class SetupScreen {
>     sf::Font& m_font;   // reference to font that lives in main()
>     sf::Text  m_label;  // safe: m_font outlives SetupScreen
> };
> ```
>
> The pattern used in this project: load `sf::Font` once in `main()` and pass a reference into every UI class. All `sf::Text` objects in those classes share the same font via the reference.

> **Concept: Hit-testing with `sf::FloatRect`**
>
> Every `sf::Shape` has `getGlobalBounds()` which returns an `sf::FloatRect` describing its screen position and size. Use `contains()` to check whether a mouse click landed inside:
>
> ```cpp
> sf::RectangleShape button(sf::Vector2f(80, 30));
> button.setPosition(200, 150);
>
> if (event.type == sf::Event::MouseButtonPressed) {
>     float mx = static_cast<float>(event.mouseButton.x);
>     float my = static_cast<float>(event.mouseButton.y);
>     if (button.getGlobalBounds().contains(mx, my)) {
>         // button was clicked
>     }
> }
> ```
>
> `getGlobalBounds()` accounts for any transforms (position, scale, rotation) applied to the shape, so the bounds are always in window coordinates — the same coordinate space as `event.mouseButton.x/y`.

> **Concept: `SetupScreen::run()` returns `GameConfig` by value**
>
> `run()` is a synchronous blocking call — it doesn't return until the user clicks Start. Returning `GameConfig` by value keeps the interface clean: the caller doesn't need to deal with `nullptr` checks or output parameters. The loop sets a `bool done = false` flag when Start is clicked, and the loop condition becomes `while (window.isOpen() && !done)`.

**Questions — think through these before checking answers:**
1. `window.pollEvent` vs `window.waitEvent` — what is the difference and which one belongs in a game loop?
2. `sf::Font` is passed as a reference (`sf::Font&`) rather than being a member variable (`sf::Font m_font`). What would happen if each UI class loaded its own copy of the font from disk?

</details>

<details>
<summary>Answers</summary>

**Reference implementation (`SetupScreen.cpp`):**

```cpp
#include "ui/SetupScreen.hpp"

namespace poker {

SetupScreen::SetupScreen(sf::RenderWindow& window, sf::Font& font)
    : m_window(window), m_font(font) {}

GameConfig SetupScreen::run() {
    bool done = false;

    while (m_window.isOpen() && !done) {
        sf::Event event;
        while (m_window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                m_window.close();
                return GameConfig(2, m_startingStack, m_smallBlind, m_bigBlind);
            }
            if (event.type == sf::Event::MouseButtonPressed) {
                float mx = static_cast<float>(event.mouseButton.x);
                float my = static_cast<float>(event.mouseButton.y);

                // +/- AI players
                if (m_minusAI.getGlobalBounds().contains(mx, my))
                    m_numAIPlayers = std::max(1, m_numAIPlayers - 1);
                if (m_plusAI.getGlobalBounds().contains(mx, my))
                    m_numAIPlayers = std::min(7, m_numAIPlayers + 1);

                // Start button
                if (m_startButton.getGlobalBounds().contains(mx, my))
                    done = true;
            }
        }
        draw();
        m_window.display();
    }

    return GameConfig(m_numAIPlayers + 1, m_startingStack, m_smallBlind, m_bigBlind);
}

void SetupScreen::draw() {
    m_window.clear(sf::Color(30, 30, 30));

    auto makeText = [&](const std::string& str, float x, float y, unsigned size = 24) {
        sf::Text t(str, m_font, size);
        t.setPosition(x, y);
        return t;
    };

    m_window.draw(makeText("Texas Hold'em AI Pro", 200, 60, 32));
    m_window.draw(makeText("AI Players: " + std::to_string(m_numAIPlayers), 200, 160));

    m_minusAI.setSize({30, 30});  m_minusAI.setPosition(160, 160);
    m_plusAI.setSize({30, 30});   m_plusAI.setPosition(370, 160);
    m_window.draw(m_minusAI);
    m_window.draw(makeText("-", 170, 162));
    m_window.draw(m_plusAI);
    m_window.draw(makeText("+", 380, 162));

    m_startButton.setSize({120, 40});
    m_startButton.setPosition(220, 320);
    m_startButton.setFillColor(sf::Color(50, 150, 50));
    m_window.draw(m_startButton);
    m_window.draw(makeText("Start", 252, 328));
}

} // namespace poker
```

*(The reference above is intentionally simplified — real implementations vary. The key requirement is that `run()` returns `GameConfig` and the loop follows: poll → handle → draw → display.)*

---

**Q1.** `pollEvent` returns immediately with `false` if the event queue is empty — the loop keeps spinning at full CPU speed (or as fast as `display()` allows). `waitEvent` blocks until an event arrives — CPU usage drops to near zero between events but the screen stops updating. In a game loop where you need to render every frame, `pollEvent` is correct. `waitEvent` is appropriate for event-driven tools (text editors, configuration dialogs) where nothing needs to be redrawn unless something changes. `SetupScreen` is borderline — it could use `waitEvent` since nothing animates — but using `pollEvent` keeps the same loop structure as `GameRenderer`, simplifying Phase 8 wiring.

**Q2.** Loading a font from disk is an I/O operation — slow (milliseconds) and resource-heavy (typically 100–300 KB per font file). Loading one copy per class would multiply that cost across `SetupScreen`, `GameRenderer`, and `InputHandler`. More importantly, three separate `sf::Font` instances mean three independent copies in memory and three separate texture uploads to the GPU, causing visible inconsistency if one load fails. One font loaded in `main()`, shared via reference, is the correct pattern.

</details>

---

### Task 7.2: Implement `GameRenderer`

`GameRenderer` takes a `GameState` snapshot and draws the entire table: players, chips, community cards, pot, and the human player's hole cards. It deliberately receives a *copy* of the state, not a reference to the live engine state — this is how the UI stays thread-safe without holding a lock during the entire render pass.

- [ ] Create `src/ui/GameRenderer.hpp`:

```cpp
#pragma once
#include "core/GameState.hpp"
#include <SFML/Graphics.hpp>

namespace poker {

class GameRenderer {
public:
    GameRenderer(sf::RenderWindow& window, sf::Font& font);

    // Draws one frame from a state snapshot.
    // snapshot is a value copy — safe to read with no locks held.
    void render(const GameState& snapshot, PlayerId humanId);

private:
    void drawTable();
    void drawPlayer(PlayerId id, const GameState& state, sf::Vector2f pos);
    void drawCard(const Card& card, sf::Vector2f pos);
    void drawCommunityCards(const GameState& state);
    void drawPot(const GameState& state);

    sf::RenderWindow& m_window;
    sf::Font&         m_font;
};

} // namespace poker
```

- [ ] Implement `src/ui/GameRenderer.cpp`. **Your turn.** Key points:
  - `render()` calls `m_window.clear()` at the start
  - Use `sf::RectangleShape` for cards — fill with white, draw rank+suit as `sf::Text` on top
  - Mark the active player visually (e.g. highlighted name or border)
  - Show `"Thinking..."` text when `state.waitingForAction` is true and the active player is not the human
  - Do **not** call `m_window.display()` — the caller owns the final swap

<details>
<summary>Concepts</summary>

> **Concept: Why render from a snapshot, not a live reference**
>
> `GameEngine::getStateSnapshot()` returns a `GameState` by value — a full copy protected by a mutex:
>
> ```cpp
> GameState GameEngine::getStateSnapshot() const {
>     std::lock_guard<std::mutex> lock(m_stateMutex);
>     return m_state;  // copy made while lock is held
> }
> ```
>
> The renderer then works on that copy with no lock held. If the renderer held a reference to `m_state` directly, either:
> - It would need to hold the mutex for the entire render pass (blocking the engine for ~16ms every frame), or
> - It would risk reading partially-written state (data race → undefined behaviour)
>
> The copy approach trades a small allocation for complete thread safety with minimal lock contention.

> **Concept: `sf::RectangleShape` for cards**
>
> Card sprites are not needed for a playable game. A white rectangle with rank and suit as text works well:
>
> ```cpp
> void GameRenderer::drawCard(const Card& card, sf::Vector2f pos) {
>     sf::RectangleShape rect(sf::Vector2f(50, 70));
>     rect.setPosition(pos);
>     rect.setFillColor(sf::Color::White);
>     rect.setOutlineColor(sf::Color::Black);
>     rect.setOutlineThickness(1);
>     m_window.draw(rect);
>
>     sf::Text label(card.toShortString(), m_font, 18);
>     label.setFillColor(
>         (card.getSuit() == Suit::Hearts || card.getSuit() == Suit::Diamonds)
>             ? sf::Color::Red : sf::Color::Black
>     );
>     label.setPosition(pos.x + 5, pos.y + 5);
>     m_window.draw(label);
> }
> ```

> **Concept: Player seat positions**
>
> With up to 8 players, seat positions are typically computed by distributing them around an ellipse. A simple approach for a fixed number of seats:
>
> ```cpp
> static const std::vector<sf::Vector2f> SEAT_POSITIONS = {
>     {400, 500},   // bottom-centre (human player)
>     {650, 420},   // right
>     {700, 250},   // top-right
>     {400, 150},   // top-centre
>     {100, 250},   // top-left
>     {150, 420},   // left
> };
> ```
>
> Iterate `state.actionOrder` and map each `PlayerId` to a seat position via `m_seatOf[id]`. The engine already assigns seats — you can use the index in `actionOrder` as a seat offset from the dealer.

> **Concept: `state.waitingForAction` — the "Thinking..." signal**
>
> `GameEngine` sets `state.waitingForAction = true` just before calling `IPlayer::getAction()`, and `false` immediately after it returns. The renderer uses this to show a "Thinking..." overlay on the active AI player's seat. Because `GameRenderer` reads a snapshot, the flag reflects the state at the moment the snapshot was taken — precise enough for display purposes.

**Questions — think through these before checking answers:**
1. `render()` calls `m_window.clear()` but not `m_window.display()`. Why is `display()` intentionally left to the caller?
2. Why does `render()` take `PlayerId humanId` as a parameter rather than storing it as a member?

</details>

<details>
<summary>Answers</summary>

**Reference implementation (`GameRenderer.cpp`) — skeleton:**

```cpp
#include "ui/GameRenderer.hpp"

namespace poker {

GameRenderer::GameRenderer(sf::RenderWindow& window, sf::Font& font)
    : m_window(window), m_font(font) {}

void GameRenderer::render(const GameState& state, PlayerId humanId) {
    m_window.clear(sf::Color(0, 100, 0));  // green felt

    drawTable();
    drawCommunityCards(state);
    drawPot(state);

    // Seat positions — simple fixed layout for up to 6 players
    static const std::vector<sf::Vector2f> seats = {
        {370, 480}, {620, 380}, {620, 180},
        {370, 80},  {120, 180}, {120, 380},
    };
    int seatIdx = 0;
    for (PlayerId id : state.actionOrder) {
        if (seatIdx < static_cast<int>(seats.size()))
            drawPlayer(id, state, seats[seatIdx++]);
    }

    // Show human hole cards
    if (state.holeCards.count(humanId)) {
        const Hand& hand = state.holeCards.at(humanId);
        drawCard(hand.first,  {310, 560});
        drawCard(hand.second, {370, 560});
    }

    // "Thinking..." overlay
    if (state.waitingForAction && state.activePlayer != humanId) {
        sf::Text thinking("Thinking...", m_font, 20);
        thinking.setFillColor(sf::Color::Yellow);
        thinking.setPosition(10, 10);
        m_window.draw(thinking);
    }
    // Note: caller calls m_window.display()
}

void GameRenderer::drawTable() {
    sf::RectangleShape table(sf::Vector2f(600, 300));
    table.setPosition(100, 150);
    table.setFillColor(sf::Color(0, 120, 0));
    table.setOutlineColor(sf::Color(101, 67, 33));
    table.setOutlineThickness(8);
    m_window.draw(table);
}

void GameRenderer::drawPlayer(PlayerId id, const GameState& state, sf::Vector2f pos) {
    bool folded = state.foldedPlayers.count(id) > 0;
    bool active = state.activePlayer == id;

    sf::RectangleShape bg(sf::Vector2f(110, 50));
    bg.setPosition(pos);
    bg.setFillColor(active ? sf::Color(200, 180, 0) : sf::Color(60, 60, 60));
    m_window.draw(bg);

    int chips = state.chipCounts.count(id) ? state.chipCounts.at(id) : 0;
    int bet   = state.currentBets.count(id) ? state.currentBets.at(id) : 0;

    sf::Text info("P" + std::to_string(id) + (folded ? " [F]" : "")
                  + "\n$" + std::to_string(chips) + " | bet:" + std::to_string(bet),
                  m_font, 14);
    info.setPosition(pos.x + 4, pos.y + 4);
    m_window.draw(info);
}

void GameRenderer::drawCard(const Card& card, sf::Vector2f pos) {
    sf::RectangleShape rect(sf::Vector2f(50, 70));
    rect.setPosition(pos);
    rect.setFillColor(sf::Color::White);
    rect.setOutlineColor(sf::Color::Black);
    rect.setOutlineThickness(1);
    m_window.draw(rect);

    bool red = card.getSuit() == Suit::Hearts || card.getSuit() == Suit::Diamonds;
    sf::Text label(card.toShortString(), m_font, 18);
    label.setFillColor(red ? sf::Color::Red : sf::Color::Black);
    label.setPosition(pos.x + 5, pos.y + 5);
    m_window.draw(label);
}

void GameRenderer::drawCommunityCards(const GameState& state) {
    float x = 210;
    for (const Card& c : state.communityCards) {
        drawCard(c, {x, 290});
        x += 60;
    }
}

void GameRenderer::drawPot(const GameState& state) {
    sf::Text pot("Pot: $" + std::to_string(state.pot), m_font, 22);
    pot.setFillColor(sf::Color::White);
    pot.setPosition(350, 245);
    m_window.draw(pot);
}

} // namespace poker
```

---

**Q1.** `InputHandler::drawButtons()` also needs to draw onto the window in the same frame, after the renderer has drawn the table. If `render()` called `display()`, the buttons drawn by `InputHandler` would appear in the *next* frame rather than the current one, causing a one-frame flicker. The caller (Phase 8 main loop) owns the frame boundary: clear → render → draw buttons → display.

**Q2.** `GameRenderer` is a general table renderer — it does not conceptually "own" the knowledge of who the human player is. Passing `humanId` as a parameter keeps the renderer reusable (e.g. a spectator view, or a test that renders from a bot's perspective). Storing it as a member would couple the renderer to a specific player identity for its entire lifetime.

</details>

---

### Task 7.3: Implement `InputHandler`

`InputHandler` handles the human player's turn: it draws Fold/Call/Raise buttons when `HumanPlayer::isWaitingForInput()` is true, hit-tests mouse clicks, and calls `humanPlayer.provideAction()` to unblock the engine thread.

- [ ] Create `src/ui/InputHandler.hpp`:

```cpp
#pragma once
#include "players/HumanPlayer.hpp"
#include "core/PlayerView.hpp"
#include <SFML/Graphics.hpp>

namespace poker {

class InputHandler {
public:
    InputHandler(sf::RenderWindow& window, sf::Font& font, HumanPlayer& player);

    // Call once per frame — draws action buttons when it is the human's turn
    void drawButtons(const GameState& state);

    // Pass every sf::Event from the main loop here
    void handleEvent(const sf::Event& event, const GameState& state);

private:
    Action buildAction(const std::string& label, const GameState& state) const;

    sf::RenderWindow& m_window;
    sf::Font&         m_font;
    HumanPlayer&      m_humanPlayer;

    sf::RectangleShape m_foldBtn;
    sf::RectangleShape m_callBtn;
    sf::RectangleShape m_raiseBtn;
};

} // namespace poker
```

- [ ] Implement `src/ui/InputHandler.cpp`. **Your turn.** Key points:
  - `drawButtons()`: check `m_humanPlayer.isWaitingForInput()` first — draw nothing if false
  - Position three buttons at the bottom of the window (Fold, Call, Raise)
  - Label the Call button with the call cost from `state` — find the `callCost` using the active player's `currentBets`
  - `handleEvent()`: on `MouseButtonPressed`, call `buildAction()` based on which button was hit, then call `m_humanPlayer.provideAction(action)`
  - `buildAction()`: return `Fold`, `Call`, or `Raise{minRaiseTo}` — read amounts from `state`

<details>
<summary>Concepts</summary>

> **Concept: Guard every action path with `isWaitingForInput()`**
>
> `drawButtons()` and the click-handler in `handleEvent()` must both guard on `isWaitingForInput()`:
>
> ```cpp
> void InputHandler::drawButtons(const GameState& state) {
>     if (!m_humanPlayer.isWaitingForInput()) return;  // ← guard
>     // draw buttons...
> }
>
> void InputHandler::handleEvent(const sf::Event& event, const GameState& state) {
>     if (!m_humanPlayer.isWaitingForInput()) return;  // ← guard
>     if (event.type != sf::Event::MouseButtonPressed)  return;
>     // hit-test and call provideAction...
> }
> ```
>
> Without the guard in `handleEvent()`, a stray click during an AI turn calls `m_promise.set_value()` on a promise that is not currently being waited on — corrupting the next human turn.

> **Concept: Reading call cost and raise bounds from `GameState`**
>
> `InputHandler` does not have access to a `PlayerView` (that is internal to the engine). It must compute action costs from the raw `GameState`:
>
> ```cpp
> int maxBet = 0;
> for (auto& [id, bet] : state.currentBets)
>     maxBet = std::max(maxBet, bet);
>
> int myBet    = state.currentBets.count(humanId) ? state.currentBets.at(humanId) : 0;
> int myChips  = state.chipCounts.count(humanId)  ? state.chipCounts.at(humanId)  : 0;
> int callCost = maxBet - myBet;
> int minRaise = maxBet + state.minRaise;
> ```
>
> This is the same arithmetic that `makePlayerView()` does — but since `InputHandler` doesn't call into the engine, it re-derives the values from the snapshot.

> **Concept: Raise amount input — keep it simple**
>
> A full raise input (text box + confirm) adds significant complexity. For Phase 7, raise to `minRaiseTo` automatically:
>
> ```cpp
> // Simple: raise button always raises to the minimum legal amount
> return Action{Action::Type::Raise, minRaiseTo};
> ```
>
> A proper raise slider or text field can be added in Phase 9 (polish). The game is fully playable with min-raise only — the human can call or fold at any time.

**Questions — think through these before checking answers:**
1. `handleEvent()` calls `m_humanPlayer.provideAction(action)`, which calls `m_promise.set_value()` on the engine worker thread's promise. Is this a data race?
2. Why does `InputHandler` hold `HumanPlayer&` (a concrete type) rather than `IPlayer&`?

</details>

<details>
<summary>Answers</summary>

**Reference implementation (`InputHandler.cpp`):**

```cpp
#include "ui/InputHandler.hpp"

namespace poker {

InputHandler::InputHandler(sf::RenderWindow& window, sf::Font& font, HumanPlayer& player)
    : m_window(window), m_font(font), m_humanPlayer(player) {
    m_foldBtn.setSize({100, 40});
    m_callBtn.setSize({100, 40});
    m_raiseBtn.setSize({100, 40});

    m_foldBtn.setPosition( 100, 630);
    m_callBtn.setPosition( 250, 630);
    m_raiseBtn.setPosition(400, 630);
}

void InputHandler::drawButtons(const GameState& state) {
    if (!m_humanPlayer.isWaitingForInput()) return;

    PlayerId id = state.activePlayer;
    int maxBet  = 0;
    for (auto& [pid, bet] : state.currentBets)
        maxBet = std::max(maxBet, bet);
    int myBet    = state.currentBets.count(id) ? state.currentBets.at(id) : 0;
    int myChips  = state.chipCounts.count(id)  ? state.chipCounts.at(id)  : 0;
    int callCost = maxBet - myBet;
    int minRaise = maxBet + state.minRaise;

    auto drawBtn = [&](sf::RectangleShape& btn, const std::string& label, sf::Color col) {
        btn.setFillColor(col);
        m_window.draw(btn);
        sf::Text t(label, m_font, 16);
        t.setPosition(btn.getPosition().x + 8, btn.getPosition().y + 10);
        m_window.draw(t);
    };

    drawBtn(m_foldBtn,  "Fold",
            sf::Color(180, 60, 60));
    drawBtn(m_callBtn,  callCost == 0 ? "Check" : "Call $" + std::to_string(callCost),
            sf::Color(60, 120, 180));
    drawBtn(m_raiseBtn, minRaise <= myChips + myBet ? "Raise $" + std::to_string(minRaise) : "All-in",
            sf::Color(180, 140, 40));
}

void InputHandler::handleEvent(const sf::Event& event, const GameState& state) {
    if (!m_humanPlayer.isWaitingForInput()) return;
    if (event.type != sf::Event::MouseButtonPressed) return;

    float mx = static_cast<float>(event.mouseButton.x);
    float my = static_cast<float>(event.mouseButton.y);

    if (m_foldBtn.getGlobalBounds().contains(mx, my)) {
        m_humanPlayer.provideAction(Action{Action::Type::Fold});
    } else if (m_callBtn.getGlobalBounds().contains(mx, my)) {
        m_humanPlayer.provideAction(Action{Action::Type::Call});
    } else if (m_raiseBtn.getGlobalBounds().contains(mx, my)) {
        PlayerId id  = state.activePlayer;
        int maxBet   = 0;
        for (auto& [pid, bet] : state.currentBets) maxBet = std::max(maxBet, bet);
        int minRaise = maxBet + state.minRaise;
        m_humanPlayer.provideAction(Action{Action::Type::Raise, minRaise});
    }
}

} // namespace poker
```

---

**A1.** No — by design. `provideAction()` calls `m_promise.set_value()`. The promise was placed there by the engine worker thread inside `HumanPlayer::getAction()`, which is blocked on `m_future.get()`. `set_value()` is thread-safe in the C++ standard: exactly one call to `set_value()` is allowed, and it synchronises with the corresponding `get()` call. The `m_waitingForInput` atomic flag ensures `provideAction()` is only called after the promise has been freshly created — so there is no race on the promise itself.

**A2.** `InputHandler` calls `isWaitingForInput()` and `provideAction()` — methods that exist only on `HumanPlayer`, not on `IPlayer`. The `IPlayer` interface intentionally exposes no UI-coupling methods; adding them would force every player type (AI, Network) to implement UI concepts. `InputHandler` is explicitly a HumanPlayer-specific component, so taking the concrete type directly is correct.

</details>

---

### Task 7.4: Wire up CMake

- [ ] Create `src/ui/CMakeLists.txt`:

```cmake
add_library(poker_ui STATIC
    SetupScreen.cpp
    GameRenderer.cpp
    InputHandler.cpp
)

target_include_directories(poker_ui PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(poker_ui PUBLIC
    poker_core
    poker_players
    sfml-graphics
    sfml-window
    sfml-system
)
```

- [ ] Uncomment `add_subdirectory(ui)` in `src/CMakeLists.txt`.

- [ ] Reconfigure and build:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
```

<details>
<summary>Concepts</summary>

> **Concept: `poker_ui` links `sfml-graphics`, `sfml-window`, `sfml-system`**
>
> SFML is split into modules. The ones needed here:
>
> | Module | What it provides |
> |---|---|
> | `sfml-graphics` | `sf::RenderWindow`, `sf::Text`, `sf::RectangleShape`, `sf::Font` |
> | `sfml-window` | `sf::Event`, `sf::VideoMode` — window creation and input |
> | `sfml-system` | `sf::Vector2f`, `sf::Color` — used by graphics and window |
>
> `sfml-graphics` depends on `sfml-window` which depends on `sfml-system`, but listing all three explicitly avoids relying on transitive propagation, which can be fragile across different SFML versions.

> **Concept: Library name `poker_ui`**
>
> Consistent with `poker_core`, `poker_ai`, `poker_players`. The `poker_` prefix avoids clashes with any CMake built-in or system target named `ui`.

**Questions — think through these before checking answers:**
1. `InputHandler.cpp` includes `HumanPlayer.hpp` which includes `IPlayer.hpp`. Does `poker_ui` need to explicitly list `poker_players` in `target_link_libraries`, or is it implied?
2. SFML was fetched via `FetchContent` in the root `CMakeLists.txt`. Why do the SFML target names (`sfml-graphics` etc.) not need the `sfml::` namespace prefix used by some other packages?

</details>

<details>
<summary>Answers</summary>

**A1.** It must be listed explicitly. `poker_players` is a CMake target defined in this project — it is not automatically on the include or link path just because a header transitively includes a file from it. `target_link_libraries(poker_ui PUBLIC poker_players)` makes the headers visible at compile time and the library symbols available at link time. Omitting it causes "undefined reference" linker errors for any `HumanPlayer` or `IPlayer` symbols used in the UI object files.

**A2.** SFML's CMake integration uses plain target names (`sfml-graphics`) rather than namespaced ones (`sfml::graphics`). This is a historical SFML convention — namespaced targets (`sfml::graphics`) were added in SFML 3.x. Since this project uses SFML 2.6 via `FetchContent`, the old-style target names apply. `nlohmann_json` and `httplib` use `::` namespacing because they follow modern CMake conventions introduced after SFML 2.x.

</details>

---

### Task 7.5: Build and run

- [ ] Build:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
```

- [ ] Run tests — the UI layer has no unit tests, but all previous suites must still pass:

```bash
ctest --test-dir build --output-on-failure
```

- [ ] Run the binary — at this stage it will open a window and show the setup screen, but clicking Start does nothing until Phase 8 wires the engine:

```bash
./build/texas-holdem
```

- [ ] Commit:

```bash
git add src/ui/ src/CMakeLists.txt
git commit -m "feat(ui): add SetupScreen, GameRenderer, InputHandler"
```

---

## Evaluation

Run `/phase-verify` to compile and get automated feedback on your implementation.

### Build checklist

- [ ] `cmake --build` exits with code `0`; `Built target poker_ui` appears in the output
- [ ] No `undefined reference` linker errors for SFML symbols
- [ ] `ctest` still passes all previous tests (core, ai, players)
- [ ] `./build/texas-holdem` opens a window without crashing

### Concept checklist

- [ ] Does `GameRenderer::render()` accept `const GameState&` (snapshot copy), not a pointer to live engine state?
- [ ] Does `GameRenderer::render()` call `m_window.clear()` but **not** `m_window.display()`?
- [ ] Does `InputHandler` guard every draw and click-handle path with `isWaitingForInput()`?
- [ ] Is `sf::Font` stored outside `GameRenderer` and `InputHandler` (loaded once in `main()`, passed by reference)?
- [ ] Does `SetupScreen::run()` return `GameConfig` by value?
- [ ] Are all `sf::RenderWindow` and `sf::Event` calls on the main thread only?
- [ ] Is `add_subdirectory(ui)` uncommented in `src/CMakeLists.txt`?

### Common mistakes

| Mistake | Symptom | Fix |
|---|---|---|
| `sf::Font` stored as a local variable inside a draw function | Text renders as blank white rectangles | Store `sf::Font` as a member or load once in `main()` and pass by reference |
| `render()` calls `window.display()` | Buttons drawn by `InputHandler` appear one frame late | Remove `display()` from `render()`; let the main loop call it after all drawing is done |
| No `isWaitingForInput()` guard in `handleEvent()` | Clicking during AI turns calls `set_value()` on a stale promise; next human turn hangs | Add `if (!m_humanPlayer.isWaitingForInput()) return;` at the top of `handleEvent()` |
| Missing `sfml-graphics` in `target_link_libraries` | Linker errors: undefined reference to `sf::Text`, `sf::RectangleShape` | Add `sfml-graphics sfml-window sfml-system` to `poker_ui`'s `target_link_libraries` |
| Calling `window.draw()` directly from `InputHandler` constructor | Constructor runs before window is ready | Only draw inside methods called from the main loop, never in constructors |
