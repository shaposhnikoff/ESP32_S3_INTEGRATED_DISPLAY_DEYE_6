# Test results (2026-11-20)

Results from the final development checks and the connected 5B board.

| Check | Result |
|---|---|
| Core unit tests, macOS clang + ASan/UBSan (`scripts/host_tests.sh`) | **304 checks, 0 failures; 1/1 CTest target passed** |
| Mock backend and HTTP-to-C integration pytest | **13 passed** |
| Real LVGL UI on host memory display, ASan/UBSan | **10 screens rendered; double-confirm, PIN expiry and capture-access regressions passed** |
| ruff check / format (mock) | clean |
| clang-format 19.1.7 `--check` | clean |
| cppcheck 2.13 (warning, performance, portability) | 0 findings |
| shellcheck 0.10 `scripts/*.sh` | clean |
| actionlint 1.7.8 `.github/workflows/ci.yml` | clean |
| Firmware build (espressif/idf:v5.5.1, `-Wall -Wextra -Werror` on first-party code) | success, 0 warnings |
| App image | 1,643,584 B, 61 % of the 4 MB slot free |
| Release packaging + image validation (project name, version 0.1.0, hash valid) | pass |
| Clean-configure dependency regression | unpatched SDK: 16 -> 25 dependencies and LVGL 9.6.0; patched SDK: 16 -> 16 and LVGL 9.3.0; same source revision |
| Patched helper build and canonical lock preservation | build exited 0; lock byte-for-byte equal to clean patched configure |
| License delivery in release archive | project `LICENSE`/`NOTICE` and LVGL/dependency license files verified inside tar.gz |
| GitHub Actions | **run `44744552799`, revision `b5d2f5a`: all four jobs passed**, including unchanged lock, package/image validation and firmware upload |
| Flash via native USB-Serial/JTAG | **passed**, image hashes verified, automatic reset |
| Boot, RGB display, GT911 initialization | **passed** in boot logs |
| File profile, NVS reload, automatic Wi-Fi/DHCP and real telemetry | **passed** in boot logs and device captures |
| Device HTTP `/health`, `/metrics`, `/screenshot.bmp` | **passed**, real 1024x600 BMP frames converted to PNG |
| Protected Controls capture | **503 rejected**, as intended |
| Physical touch, 24 h soak, static IP, OTA/rollback | **not executed**, see hardware-validation.md |

The host run used the already cached CMake binary on `PATH`; no global CMake
installation was needed. The UI harness ran in the ESP-IDF container:

```bash
cmake -S tests/ui -B build-ui
cmake --build build-ui -j 8
mkdir -p build-ui/screens
./build-ui/ui_smoke tests/fixtures/metrics_live_2026-11-20.txt build-ui/screens
```

Static checks: `scripts/format.sh --check`, `scripts/static_analysis.sh`,
`shellcheck scripts/*.sh`, and `actionlint .github/workflows/ci.yml`.
cppcheck ran in Ubuntu 24.04. Firmware build and packaging commands were
`scripts/idf.sh build` and `scripts/package_release.sh`; both exited 0.

The clean-build regression is fixed in the project Dockerfile's SDK CMake
filter, not by changing LVGL versions or suppressing compiler diagnostics.
See completion-audit.md for the failure mechanism and toggle evidence.

CI artifact: `deye-hmi-firmware`, ID `14632149750`, 4,101,994 bytes,
source `b5d2f5a6e24034c79ab82d047a47a45794030bbf`.
Upload digest:
`sha256:9d6f4323e4af70aa19279b317522434fad4a4c0ec0931a09f74e766eb9e624a7`.
It contains the unprovisioned firmware bundle and license files; private
network credentials are not part of the CI checkout.

The subsequent independent review identified three open findings, documented
in backlog.md and independent-review.md. Existing passing tests do not cover
open-dialog expiry or an endpoint change during an in-flight HTTP request.
No fixes or new regression-test passes are claimed for these deferred items.

Container activation printed a non-fatal warning that UID 501 has no username
inside the image; compilation and packaging succeeded.
