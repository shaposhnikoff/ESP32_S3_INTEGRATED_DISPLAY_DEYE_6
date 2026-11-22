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

The executable checks that only Overview is constructed, writes a PPM render,
and captures normal battery, grid, low-SOC, fault, offline and
schedule-unavailable states (`state-*.ppm`). It checks duplicate keyboard
confirmation and rejects captures of unavailable pages under ASan/UBSan.
These are simulated host results, not hardware screenshots.
