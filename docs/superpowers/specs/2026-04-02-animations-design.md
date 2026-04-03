# Animation & Polish System — Design Spec

**Date:** 2026-04-02  
**Scope:** Add balanced (200–350ms) animations, a winner spotlight, action badge transitions, and sound effects to the existing SFML render loop. Prefer linear interpolation and simple alpha math throughout — no complex easing or multi-phase timing.

---

## Goals

- Card dealing, chip movement, winner spotlight, action badge transitions, and street transitions all animate smoothly.
- Four sound effects: deal, chip, fold, win.
- Animations always play to completion — not skippable.
- Clean, simple implementation — no particle effects, no 3D, no over-engineering.

---

## Architecture

Three components slot into the existing render loop. `GameEngine` and all game logic are untouched.

```
main loop (per frame):
  dt = clock.restart()
  snapshot = engine.getStateSnapshot()

  renderer.detectDelta(snapshot, animMgr, soundMgr)   // diff → enqueue anims, fire sounds
  animMgr.update(dt)                                   // advance animations

  renderer.render(snapshot, humanId, animMgr)          // static world (masked elements skipped)
                                                       // animMgr.draw() called at end of render()
  inputHandler.drawButtons(snapshot)
  window.display()
```

---

## AnimationManager (`src/ui/AnimationManager.hpp/cpp`)

### Anim struct

```cpp
enum class AnimType { SlideCard, SlideChip, FadeBadge, FloatText, FadePanel };

struct Anim {
    AnimType    type;
    sf::Vector2f from, to;       // world positions
    float        duration;       // seconds
    float        elapsed = 0.f;
    float        delay   = 0.f;  // seconds before this anim starts

    // Payload (only relevant fields used per type)
    Card        card;            // SlideCard
    int         amount;          // SlideChip, FloatText
    PlayerId    playerId;        // FadeBadge, PulsePanel, SlideChip
    std::string maskKey;         // non-empty → removed from mask on completion
    std::string text;            // FloatText
};
```

### Interface

```cpp
class AnimationManager {
public:
    void enqueue(Anim anim);
    void update(sf::Time dt);
    void draw(sf::RenderWindow& window, sf::Font& font,
              const std::map<std::pair<Rank,Suit>, sf::Texture>& cardTextures,
              const sf::Texture& chipTexture);
    bool isMasked(const std::string& key) const;
    bool hasActive() const;

private:
    std::vector<Anim>              m_anims;
    std::unordered_set<std::string> m_masked;
};
```

### update(dt) behaviour

- For each `Anim`: if `delay > 0`, decrement delay and skip; otherwise increment `elapsed`.
- Progress `t = clamp(elapsed / duration, 0, 1)` — plain linear interpolation.
- When `elapsed >= duration`: remove from `m_anims`, erase `maskKey` from `m_masked`.

### Mask keys

| Key format | Meaning |
|---|---|
| `card:community:N` | Community card at index N is still sliding in |
| `card:hole:P:0/1` | Hole card 0 or 1 for player P is sliding in |
| `badge:P` | Action badge for player P is animating (FadeBadge draws it) |

Chip slides and float text have no mask key — they are purely additive overlays.

### draw() rendering per type

| Type | Rendering |
|---|---|
| `SlideCard` | Card sprite interpolated from `from` → `to` at progress `t` |
| `SlideChip` | Chip sprite (top-view asset) interpolated from `from` → `to` |
| `FadeBadge` | Badge text + backing rect; alpha linearly fades from 255 → 0 over the full duration |
| `FloatText` | `"+$N"` text moves upward 30px and fades out linearly; golden color |
| `FadePanel` | Semi-transparent golden rect over winner panel; alpha linearly fades 150 → 0 over 800ms |

---

## Delta Detection (`GameRenderer::detectDelta`)

Called once per frame with the new snapshot. Diffs against `m_prevSnapshot` then sets `m_prevSnapshot = curr`.

| Condition | Animation enqueued | Sound posted |
|---|---|---|
| `communityCards.size()` grew by N | N × `SlideCard` from deck origin (400, 148) to card slot position, staggered 250ms | `deal` (once) |
| `holeCards[P]` newly present | 2 × `SlideCard` per player from dealer button position, staggered 150ms | `deal` |
| `currentBets[P]` increased | `SlideChip` from seat P position → pot centre (400, 248) | `chip` |
| `lastActions[P]` changed | `FadeBadge` for player P; if action is Fold also post `fold` sound | `fold` or — |
| `showdownWinners` newly non-empty | `SlideChip` from pot → winner seat; `FloatText` at winner seat; `FadePanel` | `win` |
| `street` changed | No animation; stale badge masks cleared | — |

**Cold-start:** On the very first frame `m_prevSnapshot` is default-constructed (empty). `detectDelta` must guard against this: if `m_prevSnapshot.chipCounts` is empty, skip all delta checks for that frame and just set `m_prevSnapshot = curr`.

**Deck origin:** `(400, 148)` — top-centre of the table felt, where the dealer conceptually sits.  
**Pot centre:** `(400, 248)` — matches `drawPot()` position.

---

## SoundManager (`src/ui/SoundManager.hpp/cpp`)

```cpp
enum class SoundEvent { Deal, Chip, Fold, Win };

class SoundManager {
public:
    void loadAll();              // loads assets/sounds/{deal,chip,fold,win}.wav
    void post(SoundEvent e);     // plays immediately; restarts if already playing
    // No flush() needed — playback is fire-and-forget per sf::Sound

private:
    sf::SoundBuffer m_buffers[4];  // one per SoundEvent value
    sf::Sound       m_sounds[4];   // one per SoundEvent; buffer outlives sound
};
```

`post()` calls `m_sounds[e].play()` directly — SFML handles concurrent playback on the same `sf::Sound` by restarting. No pool, no rate-limit bookkeeping, no pending queue.

Sound files: `assets/sounds/deal.wav`, `chip.wav`, `fold.wav`, `win.wav`.  
All CC0-licensed, sourced during implementation. Target: <100KB total, <0.5s duration each.

---

## GameRenderer changes

### Header additions

```cpp
#include "ui/AnimationManager.hpp"

// new members:
GameState m_prevSnapshot;

// new method:
void detectDelta(const GameState& curr,
                 AnimationManager& animMgr,
                 SoundManager& soundMgr);

// render() gains animMgr parameter:
void render(const GameState& snapshot, PlayerId humanId,
            AnimationManager& animMgr);
```

### render() draw order

1. `drawTable()`
2. `drawCommunityCards()` — skips cards where `animMgr.isMasked("card:community:N")`
3. `drawPot()`
4. `drawStreetLabel()`
5. `drawPlayer()` × N — skips action badge where `animMgr.isMasked("badge:P")`
6. Human hole cards — skips each where `animMgr.isMasked("card:hole:0:0")` etc.
7. `animMgr.draw(window, m_font, m_cardTextures, chipTexture)`

### Chip texture

`AnimationManager::draw()` needs a chip sprite. `GameRenderer` loads `assets/chips/blue_top_large.png` into `m_chipTexture` (alongside existing card textures) and passes it through. Scaled to ~20×20 for the in-flight sprite.

---

## main.cpp changes

```cpp
// Construct alongside renderer:
poker::AnimationManager animMgr;
poker::SoundManager     soundMgr;
soundMgr.loadAll();

// Add sf::Clock before loop:
sf::Clock frameClock;

// Each frame:
sf::Time dt = frameClock.restart();
poker::GameState snapshot = engine.getStateSnapshot();

renderer.detectDelta(snapshot, animMgr, soundMgr);  // sounds fired here immediately
animMgr.update(dt);

renderer.render(snapshot, humanId, animMgr);   // animMgr.draw() called inside render() as step 7
inputHandler.drawButtons(snapshot);
window.display();
```

---

## Files changed / created

| File | Change |
|---|---|
| `src/ui/AnimationManager.hpp` | New |
| `src/ui/AnimationManager.cpp` | New |
| `src/ui/SoundManager.hpp` | New |
| `src/ui/SoundManager.cpp` | New |
| `src/ui/GameRenderer.hpp` | Add `m_prevSnapshot`, `detectDelta()`, update `render()` signature |
| `src/ui/GameRenderer.cpp` | Implement `detectDelta()`, update `render()`, add chip texture |
| `src/ui/CMakeLists.txt` | Add new sources |
| `src/main.cpp` | Construct `AnimationManager` + `SoundManager`, wire loop |
| `assets/sounds/` | 4 × WAV (deal, chip, fold, win) |

---

## Out of scope

- 3D card flip effect
- Particle effects
- Animation skip / fast-forward
- Per-player chip stack visualisation
- Configurable animation speed
