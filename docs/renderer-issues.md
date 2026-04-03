# Renderer Code Review Issues

Found during review of `src/ui/GameRenderer.cpp/hpp`, `src/ui/InputHandler.cpp`, `src/main.cpp`.

---

## Priority 1 — Critical bug: fullscreen toggle invalidates GPU textures

**File:** `src/main.cpp` lines 179–189
`window.create()` destroys the OpenGL context. All `sf::Texture` objects in `m_cardTextures`, `m_tableTexture`, and `m_cardBackTexture` become invalid — drawing them produces white rectangles or a crash.

**Fix:** Make `loadAssets()` public (rename to `reloadAssets()`), call it from `main.cpp` after every `window.create()`:
```cpp
window.create(...);
window.setFramerateLimit(60);
window.setView(computeLetterboxView(window.getSize()));
renderer.reloadAssets();
```

---

## Priority 2 — Logic bug: all-in player rendered as "BUST"

**File:** `src/ui/GameRenderer.cpp` line 230
`busted` is inferred as `chips == 0 && bet == 0`. A player who goes all-in pre-flop has zero chips and zero current-street bet — they're still alive but receive the dark-red "BUST" panel.

**Fix:** Also check `totalContributed` — a player with a contribution is all-in, not bust:
```cpp
bool busted = (chips == 0 && bet == 0 && state.totalContributed.at(id) == 0);
```

---

## Priority 3 — Portability: hardcoded system font path

**File:** `src/main.cpp` line 63
`/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf` only exists on Debian/Ubuntu. On Fedora, Arch, or macOS the app exits immediately.

**Fix:** Ship the font under `assets/fonts/` and load it via a relative path, consistent with every other asset.

---

## Priority 4 — Dead state: `m_assetsLoaded` flag never read

**File:** `src/ui/GameRenderer.hpp` line 35, `src/ui/GameRenderer.cpp` line 120
The flag is set to `true` at the end of `loadAssets()` but is never checked anywhere. It provides no actual guard.

**Fix:** Either gate draw calls on it, or remove it entirely.

---

## Priority 5 — Magic numbers in `drawTable()`

**File:** `src/ui/GameRenderer.cpp` lines 167–168
The table sprite scale uses the literals `800.f` and `700.f` directly. `LOGICAL_SIZE` lives in `main.cpp` and isn't visible to the renderer, so the literals were duplicated.

**Fix:** Define at the top of `GameRenderer.cpp`:
```cpp
static constexpr float LOGICAL_W = 800.f;
static constexpr float LOGICAL_H = 700.f;
```
and use them in `drawTable()`.

---

## Priority 6 — Fragile: seat assignment depends implicitly on `std::map` order

**File:** `src/ui/GameRenderer.cpp` lines 135–138
Players beyond `humanId` are inserted into the seat array by iterating `state.chipCounts` (`std::map<PlayerId, int>`), which iterates in ascending key order. This is deterministic today but would silently break if `chipCounts` were changed to `std::unordered_map`.

**Fix:** Add a comment documenting the dependency, or explicitly sort the player list after building it.

---

## Priority 7 — Performance: `sf::Text` and `sf::Sprite` constructed every frame

**Files:** `src/ui/GameRenderer.cpp` (multiple sites), `src/ui/InputHandler.cpp` lines 59, 79, 95
Each `sf::Text` constructor in SFML 2 allocates memory, copies the string, and does glyph-cache lookups — multiplied across all players at 60 fps.

**Fix:** Store reusable `sf::Text` objects as members and call `setString()` / `setPosition()` per frame rather than constructing new instances. Same applies to `sf::Sprite` objects in `drawCard()` and `drawCardBack()`.

---

*Reviewed 2026-04-02*
