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

# ── GoogleTest (testing) ──────────────────────────────────
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)
FetchContent_MakeAvailable(googletest)

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
    GIT_TAG        v0.5.3.1
)
FetchContent_Populate(PokerHandEvaluator)   # downloads but does NOT call add_subdirectory
# We wire it manually in Task 1.5
```

- [ ] Create the file above
- [ ] Do NOT try to build yet — we need `src/CMakeLists.txt` first

---

### Task 1.5: Custom CMake wrapper for PokerHandEvaluator

**Concept:** Some libraries don't have a CMakeLists.txt at their repo root (PokerHandEvaluator keeps its in `cpp/`). When that's the case, `FetchContent_MakeAvailable` won't work out of the box. `FetchContent_Populate` gives you the source directory so you can define the target yourself.

The C sources live in `cpp/src/` and the headers in `cpp/include/`.

- [ ] After `FetchContent_Populate(PokerHandEvaluator)`, add this to `cmake/Dependencies.cmake`:

```cmake
# PokerHandEvaluator: manual library target
# FetchContent_Populate lowercases the name → variable is pokerevaluator_SOURCE_DIR
add_library(poker_hand_evaluator STATIC
    ${pokerevaluator_SOURCE_DIR}/cpp/src/evaluator7.c
    ${pokerevaluator_SOURCE_DIR}/cpp/src/dptables.c
    ${pokerevaluator_SOURCE_DIR}/cpp/src/tables_bitwise.c
    ${pokerevaluator_SOURCE_DIR}/cpp/src/hash.c
    ${pokerevaluator_SOURCE_DIR}/cpp/src/hashtable.c
    ${pokerevaluator_SOURCE_DIR}/cpp/src/hashtable5.c
    ${pokerevaluator_SOURCE_DIR}/cpp/src/hashtable6.c
    ${pokerevaluator_SOURCE_DIR}/cpp/src/hashtable7.c
    ${pokerevaluator_SOURCE_DIR}/cpp/src/7462.c
    ${pokerevaluator_SOURCE_DIR}/cpp/src/rank.c
)
target_include_directories(poker_hand_evaluator PUBLIC
    ${pokerevaluator_SOURCE_DIR}/cpp/include
)
```

**Why `STATIC`?** A static library (`.a`) gets copied into the final binary at link time. No `.so` file to ship alongside the executable. Look up `STATIC` vs `SHARED` and be able to explain the tradeoff.

**Why only `evaluator7.c` and not 5/6?** Texas Hold'em always evaluates 7 cards (2 hole + 5 community). The 5- and 6-card evaluators are standalone variants we don't need. Including only what you use is good practice.

**Why that `_SOURCE_DIR` variable name?** `FetchContent_Populate` lowercases the declared name when creating variables. `PokerHandEvaluator` becomes `pokerevaluator` (strips non-alphanumeric), giving `${pokerevaluator_SOURCE_DIR}`.

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
# GoogleTest provides gtest_discover_tests() via the GoogleTest module.
# FetchContent_MakeAvailable(googletest) makes it available automatically.
include(CTest)
include(GoogleTest)

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
