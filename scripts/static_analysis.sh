#!/usr/bin/env bash
# cppcheck over first-party code. Needs cppcheck (CI installs it via apt).
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."
cppcheck --error-exitcode=1 --enable=warning,performance,portability --inline-suppr \
    --std=c11 --suppress=missingIncludeSystem --suppress=unmatchedSuppression \
    -DDEYE_CORE_TESTING=1 -DFIXTURE_DIR='"fixtures"' \
    -I components/deye_core/include \
    components/deye_core/src tests/unit
