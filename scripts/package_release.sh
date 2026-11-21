#!/usr/bin/env bash
# Packages build/ outputs into dist/ with checksums and build metadata.
# Run after a successful firmware build (scripts/idf.sh build).
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."
IDF_IMAGE="${IDF_IMAGE:-deye-hmi-idf:5.5.1}"
VERSION="$(tr -d '[:space:]' < version.txt)"
OUT="dist/deye-hmi-${VERSION}"
mkdir -p "${OUT}"
cp LICENSE NOTICE "${OUT}/"
mkdir -p "${OUT}/licenses/montserrat"
cp components/ui/fonts/OFL.txt "${OUT}/licenses/montserrat/OFL.txt"
while IFS= read -r license; do
    dest="${OUT}/licenses/${license#managed_components/}"
    mkdir -p "$(dirname "${dest}")"
    cp "${license}" "${dest}"
done < <(find managed_components -type f \( -iname 'license*' -o -iname 'licence*' \
    -o -iname 'copying*' -o -iname 'ftl.txt' -o -iname 'notice*' -o -iname 'copyright*' \) | sort)

for f in build/bootloader/bootloader.bin build/partition_table/partition-table.bin \
         build/ota_data_initial.bin build/deye_hmi.bin build/flash_args; do
    test -s "$f" || { echo "missing build output: $f" >&2; exit 1; }
done
cp build/bootloader/bootloader.bin build/partition_table/partition-table.bin \
   build/ota_data_initial.bin build/deye_hmi.bin "${OUT}/"
# The bundle stores all images in one directory, unlike the build tree.
sed -e 's|bootloader/bootloader.bin|bootloader.bin|g' \
    -e 's|partition_table/partition-table.bin|partition-table.bin|g' \
    build/flash_args > "${OUT}/flash_args"

# Single image for first flash / recovery (offset 0x0).
run_idf() {
    if [[ -n "${IDF_PATH:-}" ]]; then "$@"; else
        docker run --rm -v "$PWD:/project" -w /project -e HOME=/tmp -u "$(id -u):$(id -g)" "${IDF_IMAGE}" "$@"
    fi
}
run_idf bash -c "cd build && python -m esptool --chip esp32s3 merge_bin -o ../${OUT}/deye-hmi-${VERSION}-full.bin @flash_args"

# Artifact validation: app image header must carry the expected version.
run_idf python -m esptool --chip esp32s3 image_info --version 2 "${OUT}/deye_hmi.bin" 2>/dev/null |
    tr -d '\000' | sed -n '/^Image version/,$p' > "${OUT}/image_info.txt"
grep -qx "Project name: deye_hmi" "${OUT}/image_info.txt" || { echo "unexpected project name" >&2; exit 1; }
grep -qx "App version: ${VERSION}" "${OUT}/image_info.txt" || { echo "version ${VERSION} not embedded" >&2; exit 1; }
grep -q "Validation hash: .*(valid)" "${OUT}/image_info.txt" || { echo "image hash invalid" >&2; exit 1; }

COMMIT="$(git rev-parse --short HEAD 2>/dev/null || echo unknown)"
cat > "${OUT}/build-info.json" <<EOF
{
  "name": "deye-hmi",
  "version": "${VERSION}",
  "commit": "${COMMIT}",
  "target": "esp32s3",
  "board": "Waveshare ESP32-S3-Touch-LCD-5B (1024x600)",
  "idf_image": "${IDF_IMAGE}",
  "built_utc": "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
}
EOF
if command -v sha256sum >/dev/null; then SHA=(sha256sum); else SHA=(shasum -a 256); fi
(cd "${OUT}" && "${SHA[@]}" ./*.bin build-info.json > SHA256SUMS)
(cd dist && tar -czf "deye-hmi-${VERSION}.tar.gz" "deye-hmi-${VERSION}")
echo "packaged ${OUT}"
