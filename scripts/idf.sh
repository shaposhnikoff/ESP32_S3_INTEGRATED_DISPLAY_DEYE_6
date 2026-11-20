#!/usr/bin/env bash
# Run idf.py inside the pinned ESP-IDF container.
# Usage: scripts/idf.sh build | scripts/idf.sh -p /dev/ttyUSB0 flash (Linux hosts)
set -euo pipefail
IDF_IMAGE="${IDF_IMAGE:-espressif/idf:v5.5.1}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
exec docker run --rm -t \
    -v "${ROOT}:/project" -w /project \
    -e HOME=/tmp -e IDF_COMPONENT_CACHE_PATH=/project/.idf-cache \
    -u "$(id -u):$(id -g)" \
    "${IDF_IMAGE}" idf.py "$@"
