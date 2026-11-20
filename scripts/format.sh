#!/usr/bin/env bash
# Formats (default) or checks (--check) first-party C sources with a pinned clang-format.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."
CLANG_FORMAT=(uvx --from clang-format==19.1.7 clang-format)
files=(); while IFS= read -r f; do files+=("$f"); done < <(scripts/c_sources.sh)
if [[ "${1:-}" == "--check" ]]; then
    "${CLANG_FORMAT[@]}" --dry-run -Werror "${files[@]}"
else
    "${CLANG_FORMAT[@]}" -i "${files[@]}"
fi
