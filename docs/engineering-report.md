# Engineering report: Deye HMI 0.1.0

Date: 2026-11-20.

## 1. Project summary

ESP-IDF firmware for Waveshare ESP32-S3-Touch-LCD-5B, 1024x600, using LVGL 9.
The board is flashed and displays real telemetry from an HTTP Prometheus
exporter. This is a validated monitoring prototype; the remaining hardware
checks below have not been completed.

## 2. Features and status

| Status | Features |
|---|---|
| Implemented and tested | Telemetry parser, model, freshness, bounded history, control rejection rules; ten-screen UI rendering; repeated-dialog-confirmation and basic PIN-session regressions; file-based Wi-Fi provisioning, saved NVS profile and automatic live connection; LAN screenshots with protected-screen rejection |
| Known issues deferred | Open protected dialogs outlive PIN expiry; in-flight data/history are not isolated across backend endpoint changes; release archive bytes are not reproducible |
| Implemented but hardware-unverified | Physical touch calibration and manual Wi-Fi entry, static IPv4/DNS, screen timeout/wake, PIN input on glass, OTA/rollback, factory reset, long-running stability |
| Implemented with mock backend only | HTTP timeout/offline, malformed/missing metrics, upstream Modbus failure, alarm and changing SOC scenarios through the real C telemetry parser |
| Blocked by missing external information | Inverter write API, verified writable parameters, grid import/export sign convention |
| Not implemented | Module-level BMS details, generator status absent from the exporter, server-side historical queries, active inverter writes |

## 3. Architecture

The display polls `/metrics`; it never connects to RS485 or Modbus.
The exporter remains responsible for inverter access. Platform-independent
parsing, freshness, history and command-safety logic live in `deye_core`.
FreeRTOS application tasks own networking and state; LVGL renders under its
display lock. Screenshot transfer releases that lock before sending bytes.

See architecture.md and diagrams/.

## 4. Repository structure

* `components/deye_core`: portable logic and public data contracts.
* `components/app`: Wi-Fi, settings, telemetry, diagnostics and OTA.
* `components/board`: RGB panel, backlight and touch initialization.
* `components/ui`: screens, widgets and on-device screenshot server.
* `main`: firmware entry point and Kconfig.
* `config`: safe network template; the real local profile is ignored.
* `tests`: C unit tests, HTTP bridge tests, LVGL smoke harness and fixtures.
* `backend/mock`: Python scenario server and pytest tests.
* `scripts`, `.github/workflows`: build, analysis, packaging and CI.
* `docs`: contracts, design, validation, diagrams and real screenshots.

## 5. Build commands

Requirements: Docker, uv and an ESP32-S3 board for flashing.

```bash
cp config/network.example.h config/network.local.h
# Edit the copied network profile before building.
scripts/idf.sh set-target esp32s3
scripts/idf.sh build
scripts/package_release.sh
```

The project Dockerfile builds `deye-hmi-idf:5.5.1` from the pinned
`espressif/idf:v5.5.1` base. A minimal SDK CMake filter prevents managed
components becoming local overrides during configure reruns; dependency
versions and compiler warning checks remain unchanged.
The lock serializer explicitly uses the Python YAML emitter on ARM64 and
AMD64, retaining the strict byte-for-byte lock gate.
Host tests additionally need CMake and a C compiler:

```bash
scripts/host_tests.sh
scripts/format.sh --check
scripts/static_analysis.sh
```

## 6. Flash commands

Use the USB port found on your machine. Individual partition images preserve
existing NVS settings:

```bash
cd build
uvx --from esptool==4.9.0 esptool.py --chip esp32s3 --port PORT \
  --baud 460800 --before default_reset --after hard_reset write_flash @flash_args
```

The validated connection used native USB-Serial/JTAG. See flashing.md.

## 7. Backend configuration

Provide an HTTP exporter with `/metrics` on the configured host/port
(default port 9101). Set SSID, password and exporter host in the local profile,
or configure them on the Network screen. DHCP is the default; the profile
also supports static IPv4, gateway, netmask, DNS and an NTP server.

The ignored local profile and locally provisioned binaries contain credentials.
Do not publish them. Fixtures use an anonymized inverter serial.

## 8. Endpoints and adapter mappings

The display consumes Prometheus `deye_*` metrics, including battery SOC,
PV/load/grid power, daily energy, device state and exporter freshness.
The exact mapping is documented in api-contract.md and backend-integration.md.
The exporter also has `/health`; UI freshness uses metric age and scrape status.

The display serves `/screenshot.bmp`, `/health` and `/metrics`.
Screenshot selection is restricted to monitoring screens; no inverter command
can be issued through these endpoints. See screenshots.md.

## 9. Tests and review

The final core test run passed **304 checks**, with ASan/UBSan.
The mock backend and HTTP-to-C integration suite passed **13 tests**.
The LVGL harness rendered and navigated all ten screens and passed dialog,
PIN-expiry and screenshot-access regressions with ASan/UBSan.
Firmware build, C/Python formatting, cppcheck, shellcheck and actionlint passed.
Detailed evidence and remaining checks are in test-results.md.
GitHub Actions run `44744552799` for source revision `b5d2f5a` passed all
four jobs, including dependency-lock preservation, release image validation
and artifact upload.

Manual code review corrected repeated text-dialog submission, PIN-session
handling and freshness handling. Device captures exposed a tab-transition
timing issue; capture now lets the normal animation finish without locking
the GUI during that wait. Agent-based review attempts did not return a usable
assessment. A subsequent direct independent review of 92 supplied source files
returned **REJECT** with three findings. The fixes are
deferred in backlog.md; source scope and limits are in independent-review.md.

## 10. Firmware binary location

* Application: `build/deye_hmi.bin`.
* Local bundle: `dist/deye-hmi-0.1.0/`.
* Full image: `dist/deye-hmi-0.1.0/deye-hmi-0.1.0-full.bin`.
* Archive: `dist/deye-hmi-0.1.0.tar.gz`.

These local artifacts are excluded from Git. CI builds artifacts from the
published source without the private network profile.
The validated CI bundle is the `deye-hmi-firmware` artifact from run
`44744552799`, artifact ID `14632149750`.

## 11. Known limitations

History starts at boot and is held in RAM. Missing values remain unavailable.
Static addressing, OTA and physical interaction checks remain unverified.
Credentials/PIN use NVS without flash encryption. Screenshot diagnostics use
unauthenticated HTTP and require a trusted LAN or disabling the feature.
Internal free heap observed through diagnostics was about 10 KB; no
long-duration heap-stability claim is made.
Known PIN-dialog and backend-endpoint-change issues remain unresolved.
Build reproducibility does not imply byte-identical release archives.
See backlog.md for the deferred corrections and regression criteria.

## 12. Hardware validation status

Passed: flashing with verified hashes, boot, display/touch initialization,
automatic Wi-Fi/DHCP, persisted profile on another boot, first successful
real telemetry, and five 1024x600 captures from the running board.

Pending: physical inspection for flicker/tearing, touch accuracy, manual setup,
Wi-Fi/exporter fault recovery on hardware, 24-hour soak, timeout/wake,
OTA/rollback and factory reset. See hardware-validation.md.

## 13. Control safety status

All shipped writable parameters remain unverified and blocked.
There is no active backend write transport. No inverter write was performed.
Mock tests cover confirmation, read-back disagreement, uncertain timeout,
duplicate requests and rejection of unsupported parameters.

## 14. Remaining dependencies and blockers

Active controls require a documented write API and approved parameter
validation. Rich BMS/generator views require additional backend fields.
Historical queries require a backend time-series store.
Hardware acceptance still requires the physical and long-running tests above.
