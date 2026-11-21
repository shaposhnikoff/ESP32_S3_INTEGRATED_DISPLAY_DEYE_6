# Host UI checks

Compiles the actual UI and LVGL with a deterministic host platform fake.
No ESP32 drivers, Wi-Fi, NVS, OTA, touch or inverter operations are exercised.

After `scripts/idf.sh build` has fetched the pinned LVGL sources:

```bash
cmake -S tests/ui -B build-ui -DCMAKE_BUILD_TYPE=Debug
cmake --build build-ui -j8
mkdir -p build-ui/screens
build-ui/ui_smoke tests/fixtures/metrics_live_2026-11-20.txt build-ui/screens
```

The executable navigates all ten screens, writes PPM renders, and separately
captures the overview in normal battery, grid, low-SOC, fault and offline
states (`state-*.ppm`). It checks duplicate keyboard confirmation under
ASan/UBSan and verifies immediate PIN enforcement and session expiry. These
are simulated host results, not hardware screenshots.
