# Phase 1 — CMake Scaffold & Project Structure

**Concepts you'll learn:** CMake basics, FetchContent, target-based dependency management, out-of-source builds.

**Previous phase:** _(none — this is the start)_
**Next phase:** [Phase 2 — Core Layer: Types, Card, Deck, Hand](phase-2-core-types-card-deck-hand.md)

---

### Task 1.1: Understand CMake before touching it

Read the guide before writing any CMake. This saves hours of confusion.

- [ ] Read: [CMake concepts you need](../guides/cmake-intro.md) ← we'll create this together as you go
- [ ] Understand the difference between `add_library`, `add_executable`, and `target_link_libraries`
- [ ] Understand what "out-of-source build" means and why we use `build/` as the build directory

**Key mental model:** CMake doesn't build your code — it generates build files (Makefiles, Ninja files) that actually build your code. You configure with CMake, then build with the generated system.

---

### Task 1.2: Create the root `CMakeLists.txt`

**Your turn.** Create `CMakeLists.txt` at the project root:

```cmake
cmake_minimum_required(VERSION 3.21)
project(TexasHoldemAIPro VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)   # use -std=c++17, not -std=gnu++17

# Put all FetchContent declarations in a separate file (cleaner)
include(cmake/Dependencies.cmake)

# Sub-targets (we'll add these as we build each layer)
add_subdirectory(src)
add_subdirectory(tests)
```

- [ ] Create the file above
- [ ] Verify it looks right — don't try to build yet, we haven't created `src/` or `tests/`

---

### Task 1.3: Create the folder skeleton

- [ ] Create all directories from the design spec:

```bash
mkdir -p src/{core,players,ai,ui}
mkdir -p tests/{core,players,ai}
mkdir -p cmake
mkdir -p config/personalities
mkdir -p assets/{cards,fonts}
```

- [ ] Create placeholder files. `main.cpp` needs a minimal `main()` or the linker will fail:

```bash
echo 'int main() { return 0; }' > src/main.cpp
touch tests/core/.gitkeep
touch tests/players/.gitkeep
touch tests/ai/.gitkeep
```

- [ ] Commit:
```bash
git add -A && git commit -m "chore: add project skeleton and root CMakeLists.txt"
```

---

### Task 1.4: Add dependencies via FetchContent

**Concept:** `FetchContent` lets CMake download and build dependencies at configure time. No manual `git clone` or system package needed (except SFML's system deps).

Create `cmake/Dependencies.cmake`:

```cmake
include(FetchContent)

# ── nlohmann/json ─────────────────────────────────────────
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# ── yaml-cpp ──────────────────────────────────────────────
FetchContent_Declare(
    yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG        0.8.0
)
FetchContent_MakeAvailable(yaml-cpp)

# ── cpp-httplib ───────────────────────────────────────────
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG        v0.18.1
)
FetchContent_MakeAvailable(httplib)

# ── Catch2 v3 (testing) ───────────────────────────────────
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.7.1
)
FetchContent_MakeAvailable(Catch2)

# ── SFML 2.6 ──────────────────────────────────────────────
# Note: SFML needs system libs. On Ubuntu/Debian:
#   sudo apt install libsfml-dev
# This FetchContent fetches SFML source but links against system OpenGL/freetype.
FetchContent_Declare(
    SFML
    GIT_REPOSITORY https://github.com/SFML/SFML.git
    GIT_TAG        2.6.2
)
FetchContent_MakeAvailable(SFML)

# ── PokerHandEvaluator ────────────────────────────────────
# This library needs a custom wrapper — see Task 1.5
FetchContent_Declare(
    PokerHandEvaluator
    GIT_REPOSITORY https://github.com/HenryRLee/PokerHandEvaluator.git
    GIT_TAG        master
)
FetchContent_Populate(PokerHandEvaluator)   # downloads but does NOT call add_subdirectory
# We wire it manually in Task 1.5
```

- [ ] Create the file above
- [ ] Do NOT try to build yet — we need `src/CMakeLists.txt` first

---

### Task 1.5: Custom CMake wrapper for PokerHandEvaluator

**Concept:** Some libraries don't have CMake support. You'll wrap them manually — this is a real-world CMake skill.

PokerHandEvaluator is a C library. We need to tell CMake how to compile it.

- [ ] After `FetchContent_Populate(PokerHandEvaluator)`, add this to `cmake/Dependencies.cmake`:

```cmake
# PokerHandEvaluator: manual library target
# FetchContent_Populate lowercases the name → variable is pokerevaluator_SOURCE_DIR
# We name the target poker_hand_evaluator (different from the FetchContent name)
# to avoid conflicts with CMake's internal registration.
add_library(poker_hand_evaluator STATIC
    ${pokerevaluator_SOURCE_DIR}/c/src/evaluator5.c
    ${pokerevaluator_SOURCE_DIR}/c/src/evaluator6.c
    ${pokerevaluator_SOURCE_DIR}/c/src/evaluator7.c
    ${pokerevaluator_SOURCE_DIR}/c/src/hash.c
    ${pokerevaluator_SOURCE_DIR}/c/src/hashtable.c
    ${pokerevaluator_SOURCE_DIR}/c/src/hashtable5.c
    ${pokerevaluator_SOURCE_DIR}/c/src/hashtable6.c
    ${pokerevaluator_SOURCE_DIR}/c/src/hashtable7.c
)
target_include_directories(poker_hand_evaluator PUBLIC
    ${pokerevaluator_SOURCE_DIR}/include
)
```

**Your turn:** Look up what `add_library(... STATIC ...)` means vs `SHARED`. Why do we use `STATIC` here?

- [ ] Add the block above to `cmake/Dependencies.cmake`

---

### Task 1.6: Create `src/CMakeLists.txt`

**Concept:** Each subdirectory can have its own `CMakeLists.txt`. The root one orchestrates; subdirectory ones define targets.

- [ ] Create `src/CMakeLists.txt`:

```cmake
# We'll add targets here as we build each layer.
# For now, just the main executable with a placeholder.

add_executable(texas-holdem src/main.cpp)

target_link_libraries(texas-holdem PRIVATE
    # layers will be linked here as we build them
)
```

**Wait** — this path is wrong. Think about it: `src/CMakeLists.txt` is already *inside* `src/`. So the path to `main.cpp` relative to this file is just `main.cpp`, not `src/main.cpp`.

**Your turn:** Fix the path and create the file correctly.

---

### Task 1.7: Create `tests/CMakeLists.txt`

- [ ] Create `tests/CMakeLists.txt`:

```cmake
# Catch2 provides a helper to register tests with CTest.
# We must add Catch2's extras directory to CMAKE_MODULE_PATH so
# include(Catch) can find the catch_discover_tests() function.
include(CTest)
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)

# Sub-test directories — uncomment as we add tests
# add_subdirectory(core)
# add_subdirectory(players)
# add_subdirectory(ai)
```

---

### Task 1.8: First configure & build

Now let's verify the scaffold compiles:

- [ ] Configure:
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
```

- [ ] Build:
```bash
cmake --build build -j$(nproc)
```

Expected: downloads dependencies, compiles successfully, produces `build/texas-holdem` binary.

- [ ] Run it:
```bash
./build/texas-holdem
```

Expected: exits immediately with code 0 (main returns 0), no crash.

- [ ] Commit:
```bash
git add -A && git commit -m "chore: wire up CMake with all dependencies via FetchContent"
```
