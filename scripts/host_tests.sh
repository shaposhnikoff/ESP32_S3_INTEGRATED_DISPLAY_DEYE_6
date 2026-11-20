#!/usr/bin/env bash
# Builds and runs host unit tests (ASan/UBSan) and the mock backend tests.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."
cmake -S tests -B build-tests -DCMAKE_BUILD_TYPE=Debug
cmake --build build-tests -j
ctest --test-dir build-tests --output-on-failure
(cd backend/mock && uv run --frozen pytest -q && uv run --frozen ruff check . && uv run --frozen ruff format --check .)
