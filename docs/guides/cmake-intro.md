# CMake Introduction — Concepts for Phase 1

This guide covers everything you need to understand before writing `CMakeLists.txt` files for this project. Read it top-to-bottom once; then use it as a reference.

---

## What CMake Actually Does

A common misconception: **CMake does not compile your code.** It generates *build files* for another tool that does.

```
Your source + CMakeLists.txt
         │
         ▼
    cmake (configure)
         │
         ▼
  Makefiles / build.ninja / .vcxproj
         │
         ▼
  make / ninja / msbuild (actually compiles)
```

This two-step model is why you run two separate commands:

```bash
cmake -S . -B build        # Step 1: configure — reads CMakeLists.txt, generates build files
cmake --build build        # Step 2: build — invokes the underlying build tool
```

You only need to re-run step 1 when `CMakeLists.txt` changes. Step 2 is what you run every time you want to recompile.

---

## Out-of-Source Builds

**Never build inside your source tree.** CMake generates a large number of files. Mixing them with your source makes `git status` noisy and `git clean` dangerous.

The `-S` and `-B` flags separate them:

```bash
cmake -S . -B build
#      ^      ^
#      |      └─ put generated files here (gitignored)
#      └─ source root is here (CMakeLists.txt lives here)
```

Your `.gitignore` should contain `build/`. All generated files stay there and never pollute your repo.

---

## Targets — The Core Concept

Everything in modern CMake revolves around **targets**. A target is a named thing CMake knows how to build:

| CMake command | Creates |
|---|---|
| `add_executable(my-app src/main.cpp)` | A binary you can run |
| `add_library(my-lib STATIC src/foo.cpp)` | A linkable library |
| `add_library(my-lib INTERFACE)` | A header-only "virtual" library |

Think of a target as a node in a dependency graph. Targets have:
- **Sources** — the `.cpp` files to compile
- **Include paths** — where to find headers
- **Link dependencies** — other targets or system libs they depend on
- **Compile options** — flags like `-Wall`, `-O2`

---

## `target_link_libraries` — Wiring Targets Together

This is the most important command you'll use:

```cmake
target_link_libraries(texas-holdem PRIVATE
    poker_core
    nlohmann_json::nlohmann_json
)
```

**What `PRIVATE` / `PUBLIC` / `INTERFACE` mean:**

| Keyword | "I need it to..." | "My dependents also need it?" |
|---|---|---|
| `PRIVATE` | compile and link myself | No |
| `PUBLIC` | compile and link myself | Yes (transitively) |
| `INTERFACE` | nothing (header-only) | Yes |

Rule of thumb for this project: use `PRIVATE` everywhere unless a header you expose to callers `#include`s a header from the dependency. If `GameEngine.hpp` includes a `nlohmann/json.hpp` type in its public API, that dependency is `PUBLIC`. Otherwise it's `PRIVATE`.

---

## `target_include_directories` — Finding Headers

```cmake
target_include_directories(poker_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
```

This tells CMake: "when compiling `poker_core` (and anything that links `PUBLIC`ly to it), add this path to the include search path."

`${CMAKE_CURRENT_SOURCE_DIR}` is the directory containing the current `CMakeLists.txt`. Using it makes paths relative to the file, not the project root.

---

## Subdirectory Structure

Each layer of the project has its own `CMakeLists.txt`. The root one orchestrates:

```
CMakeLists.txt          ← project-wide settings, includes Dependencies.cmake
├── src/
│   ├── CMakeLists.txt  ← defines the main executable
│   ├── core/
│   │   └── CMakeLists.txt  ← defines poker_core library
│   ├── players/
│   │   └── CMakeLists.txt  ← defines poker_players library
│   └── ...
└── tests/
    └── CMakeLists.txt  ← sets up GoogleTest + test targets
```

The root `CMakeLists.txt` pulls them in with:

```cmake
add_subdirectory(src)
add_subdirectory(tests)
```

And `src/CMakeLists.txt` pulls in its subdirectories:

```cmake
add_subdirectory(core)
add_subdirectory(players)
# etc.
```

**Important path gotcha:** inside `src/core/CMakeLists.txt`, paths are relative to `src/core/`, not the project root. `Types.hpp` is just `Types.hpp`, not `src/core/Types.hpp`.

---

## STATIC vs SHARED Libraries

```cmake
add_library(poker_core STATIC src/GameEngine.cpp)  # .a file
add_library(poker_core SHARED src/GameEngine.cpp)  # .so file
```

| | STATIC | SHARED |
|---|---|---|
| Extension | `.a` (Linux/Mac) | `.so` / `.dylib` / `.dll` |
| Linked into binary? | Yes — code copied into the binary | No — loaded at runtime |
| Deployment | Single binary, no extra files | Need to ship the `.so` too |
| Build speed | Slower link | Faster link |

**For this project:** use `STATIC` for all internal libraries. It produces a single self-contained binary (`texas-holdem`) with no runtime dependencies to manage. `pheval` (PokerHandEvaluator) is also built as `STATIC` by its own CMakeLists.txt for the same reason.

---

## FetchContent — Dependency Management

`FetchContent` downloads and integrates external libraries at *configure time* (when you run `cmake -S . -B build`). No manual `git clone` needed.

```cmake
include(FetchContent)

FetchContent_Declare(
    nlohmann_json                                        # name you give it
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3                               # always pin a tag, never "main"
)
FetchContent_MakeAvailable(nlohmann_json)
# After this line, the target nlohmann_json::nlohmann_json is available
```

**`FetchContent_MakeAvailable` vs `FetchContent_Populate`:**

| Command | What it does |
|---|---|
| `FetchContent_MakeAvailable(X)` | Downloads + calls `add_subdirectory` on it |
| `FetchContent_Populate(X)` | Downloads only — you wire it manually |

Use `FetchContent_Populate` when a library has no CMake support at all. You get the source directory via `${x_SOURCE_DIR}` (lowercased name + `_SOURCE_DIR`) and then call `add_library` yourself.

**`SOURCE_SUBDIR`** is a middle ground: the library *does* have a CMakeLists.txt, but not at the repo root. PokerHandEvaluator keeps it in `cpp/`:

```cmake
FetchContent_Declare(
    PokerHandEvaluator
    GIT_REPOSITORY https://github.com/HenryRLee/PokerHandEvaluator.git
    GIT_TAG        v0.5.3.1
    SOURCE_SUBDIR  cpp          # ← CMakeLists.txt is in cpp/, not the root
)
FetchContent_MakeAvailable(PokerHandEvaluator)
# Provides target: pheval
```

**Controlling a dependency's options before `MakeAvailable`:** If a library's CMakeLists.txt has `option(BUILD_TESTS ...)` that you want off, set it as a cache variable with `FORCE` *before* calling `FetchContent_MakeAvailable`:

```cmake
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(SomeLib)
```

This is important here: PokerHandEvaluator would otherwise try to fetch its own copy of GoogleTest (at a different version), causing a conflict with our `v1.14.0`.

**Always pin versions.** Using `GIT_TAG main` means your build can silently break when upstream changes. Use a release tag like `v3.11.3` or a commit SHA.

---

## CMake Variables You'll See Often

| Variable | Meaning |
|---|---|
| `CMAKE_SOURCE_DIR` | Root of the project (where root `CMakeLists.txt` is) |
| `CMAKE_CURRENT_SOURCE_DIR` | Directory of the *current* `CMakeLists.txt` |
| `CMAKE_BINARY_DIR` | Root of the build directory (`build/`) |
| `CMAKE_CXX_STANDARD` | C++ standard to use (`17`) |
| `CMAKE_BUILD_TYPE` | `Debug` / `Release` / `RelWithDebInfo` |
| `<name>_SOURCE_DIR` | Set by FetchContent for downloaded dep named `<name>` |

---

## C++ Standard Settings

```cmake
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)  # error if compiler can't do C++17
set(CMAKE_CXX_EXTENSIONS OFF)        # -std=c++17 not -std=gnu++17
```

`EXTENSIONS OFF` matters: GNU extensions add non-standard behavior that can hide portability bugs. Always turn them off.

---

## Common Mistakes to Avoid

**1. Forgetting `target_` prefix on commands**

```cmake
# Wrong — sets include path globally for everything
include_directories(include/)

# Right — scoped to a specific target
target_include_directories(poker_core PUBLIC include/)
```

Global commands like `include_directories`, `add_definitions`, `link_libraries` pollute all targets. Use the `target_` variants.

**2. Hardcoding absolute paths**

```cmake
# Wrong
target_include_directories(poker_core PUBLIC /home/x/dev/texas-holdem-aipro/src/core)

# Right
target_include_directories(poker_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
```

**3. Using wrong link scope**

If you `PRIVATE`-link a library but expose its types in your headers, callers won't be able to find those headers and will get cryptic compile errors.

**4. Building in-source**

Running `cmake .` instead of `cmake -S . -B build` spills generated files into your source tree. If this happens: add `build/` and any stray `.cmake` files to `.gitignore`, then `git clean -fd` to remove them.

---

## Quick Reference: This Project's CMake Structure

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug   # configure
cmake --build build -j$(nproc)                  # build (parallel)
cd build && ctest --output-on-failure           # run tests
./build/texas-holdem                            # run game
```

Target dependency graph (will grow as phases complete):

```
texas-holdem
├── poker_ui        (PRIVATE)
├── poker_players   (PRIVATE)
├── poker_ai        (PRIVATE)
└── poker_core      (PRIVATE)
    └── pheval (PRIVATE)  ← PokerHandEvaluator C++ target

poker_ai
├── poker_core      (PUBLIC — AI headers reference core types)
├── nlohmann_json::nlohmann_json (PRIVATE)
├── httplib::httplib             (PRIVATE)
└── yaml-cpp                     (PRIVATE)

poker_players
└── poker_core      (PUBLIC)

poker_ui
├── poker_core      (PUBLIC)
└── sfml-graphics sfml-window sfml-system (PRIVATE)
```

---

## What to Do Next

Once you've read this guide, you're ready for Phase 1, Task 1.2: writing the root `CMakeLists.txt`. The concepts above map directly to what you'll write. Refer back here whenever a command is unfamiliar.
