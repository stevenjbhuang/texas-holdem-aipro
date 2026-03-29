#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$ROOT/build-cov"
HTML_DIR="$ROOT/coverage-html"

# ── dependencies ─────────────────────────────────────────────────────────────
if ! command -v gcovr &>/dev/null; then
    echo "gcovr not found — installing..."
    python -m pip install --quiet gcovr
fi

# ── configure ────────────────────────────────────────────────────────────────
echo "Configuring with coverage flags..."
cmake -S "$ROOT" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_FLAGS="--coverage -O0" \
    -DCMAKE_EXE_LINKER_FLAGS="--coverage" \
    -DCMAKE_C_FLAGS="--coverage -O0" \
    --log-level=WARNING

# ── build ─────────────────────────────────────────────────────────────────────
echo "Building..."
cmake --build "$BUILD_DIR" -j"$(nproc)" 2>&1 | grep -E "error:|warning:|Linking|Built" || true

# ── run tests ────────────────────────────────────────────────────────────────
echo "Running tests..."
ctest --test-dir "$BUILD_DIR" --output-on-failure

# ── report ───────────────────────────────────────────────────────────────────
echo ""
echo "Coverage summary:"
gcovr \
    --root "$ROOT/src" \
    --filter "$ROOT/src" \
    --exclude-directories "$BUILD_DIR/_deps" \
    --gcov-ignore-errors=no_working_dir_found \
    --gcov-executable gcov \
    --object-directory "$BUILD_DIR" \
    --print-summary

echo ""
mkdir -p "$HTML_DIR"
echo "Generating HTML report → $HTML_DIR"
gcovr \
    --root "$ROOT/src" \
    --filter "$ROOT/src" \
    --exclude-directories "$BUILD_DIR/_deps" \
    --gcov-ignore-errors=no_working_dir_found \
    --gcov-executable gcov \
    --object-directory "$BUILD_DIR" \
    --html-details "$HTML_DIR/index.html"

echo "Done. Open: $HTML_DIR/index.html"
