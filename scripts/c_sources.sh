#!/usr/bin/env bash
# Lists first-party C sources (third-party/managed code excluded).
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."
find main components tests -name '*.[ch]' ! -path 'components/ui/fonts/deye_font_*.c' | sort
