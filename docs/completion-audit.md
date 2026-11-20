# Master-prompt completion audit

Date: 2026-11-20. Runtime source baseline: `d2c5181`.
Validated build/tooling revision: `b5d2f5a`.

The deliverable is a functioning, reproducibly buildable 5B monitoring HMI,
with a real read-only exporter adapter, mock tests, safe disabled controls,
release tooling and a section-15 engineering report. Missing backend contracts
and hardware acceptance checks are handled under the explicit exception in
master-prompt section 16; they are not claims of production readiness.

## Primary objectives and screen requirements

| Requirement | Current evidence | Status |
|---|---|---|
| Live telemetry and communication with Raspberry Pi | `components/app/telemetry.c`, typed parser/model in `components/deye_core`, real Overview capture | Implemented and tested |
| Energy flow, SOC, inverter mode, communication age | `components/ui/scr_overview.c`, `docs/screenshots/overview.png`; grid direction is not asserted before sign configuration | Implemented and tested |
| Battery V/A/W/temperature, state and history | `scr_battery.c`, `docs/screenshots/battery.png`, parser and history tests | Implemented and tested |
| PV strings, total/daily generation and history | `scr_solar.c`, `docs/screenshots/solar.png`, typed MPPT metrics | Implemented and tested |
| Grid availability/V/Hz/power and household load | `scr_grid.c`, `docs/screenshots/grid.png`; import/export sign remains unverified | Implemented and tested, sign blocked by external information |
| Generator screen and supported configuration | `scr_generator.c` displays unavailable measurements explicitly and real ToU charging flags; no invented generator telemetry or writes | Screen implemented; measurements blocked by missing backend fields |
| Interactive PV/load/grid/battery/SOC charts and four time ranges | `scr_charts.c`, bounded `history.c`, aggregation/ring tests, `docs/screenshots/charts.png` | Implemented and tested |
| Inverter configuration and bidirectional controls | `scr_controls.c`, read-only ToU/capabilities, full tested `control.c` lifecycle; all parameters remain unverified | Safety lifecycle tested with mocks; real writes blocked |
| Network selection/password/host/port/test/status | `scr_network.c`, `net_wifi.c`, settings/profile code, successful automatic live connection | Implemented; manual physical input and outage recovery hardware-unverified |
| Health, errors, alarms and diagnostics | `link_state.c`, `scr_diag.c`, status bar and alarm handling; malformed/stale/offline/timeout integration scenarios | Implemented and tested with mocks and live normal data |
| Firmware update, reboot, recovery and factory reset | `ota.c`, `scr_system.c`, rollback configuration, `docs/release.md` | Implemented but hardware-unverified |
| Persistent display settings | `settings.c`, ignored local network profile, NVS reload on another boot | Network persistence tested; remaining settings hardware-unverified |
| Theme, timeout/wake, localization and brightness | `ui_theme.c`, `ui_main.c`, `scr_system.c`, `i18n.c`; board backlight supports on/off only | Implemented; physical timeout/wake unverified; translated/Cyrillic fonts not implemented |
| Ten-screen navigation and reusable widgets | `ui_main.c`, `ui_widgets.c`, `ui_theme.c`, `tests/ui/ui_smoke.c`; all ten screens rendered | Implemented and tested |
| Module-level BMS and backend historical queries | Explicitly unavailable in `docs/api-contract.md`; aggregate data is not split into fake modules | Blocked by missing external information |
| Standalone daily operation and continuous reliability | Wi-Fi autoconnect and monitoring on the real board; watchdogs and bounded buffers configured | Basic operation tested; physical acceptance and 24-hour soak remain unverified |

Screen sources above are under `components/ui/`. The complete field-by-field
screen map is `docs/screen-map.md`. No placeholder directories were added to
imitate the suggested layout: board/display/touch share `components/board`,
transport/network/storage/OTA share `components/app`, and portable
model/telemetry/history/control share `components/deye_core`.

## Required deliverables by engineering role

| Role / master-prompt outputs | Evidence |
|---|---|
| Research | `docs/research.md`, `hardware-compatibility.md`, `references.md`, `technical-decisions.md`; exact vendor timings/pins and licensing are recorded |
| Architecture | `docs/architecture.md`, `task-model.md`, `memory-model.md`, `network-architecture.md`, `docs/adr/` |
| UI/UX | `docs/ui-design.md`, `screen-map.md`, `design-system.md`, actual screen/widget/theme sources; built-in LVGL fonts/icons avoid empty custom asset modules |
| Backend integration | `docs/api-contract.md`, `backend-integration.md`, `components/app/telemetry.c`, portable parser/model, `backend/mock`, `tests/fixtures`; desired internal operations are mapped explicitly |
| Control safety | `docs/control-safety.md`, `writable-parameters.md`, portable `control.c`, `tests/unit/test_control.c`; no register addresses or write transport invented |
| Firmware | `main/app_main.c`, board/app/UI/core components, `sdkconfig.defaults`, `partitions.csv`, pinned manifests and `dependencies.lock` |
| QA | `tests/unit`, `tests/integration`, `tests/ui`, fixtures and mock tests; `docs/test-plan.md`, `test-results.md`, `hardware-validation.md` |
| DevOps/release | `.github/workflows/ci.yml`, executable `scripts/`, `docs/build.md`, `release.md`, `flashing.md`, release notes and local versioned artifacts |
| Independent review | Direct review of 92 supplied files returned REJECT; three findings are deferred in `docs/backlog.md`, with scope in `docs/independent-review.md` |

Paths in the documentation column without a directory prefix are relative to
`docs/`. Separate display/touch/backend/control directories were consolidated
into owning components rather than left as empty stubs.

## Quality-gate evidence

| Gate | Evidence |
|---|---|
| Correct target and reproducible dependencies | ESP32-S3, 16 MB flash, octal 8 MB PSRAM, 1024x600 RGB565, vendor 21 MHz timings; project Dockerfile based on ESP-IDF `v5.5.1`, LVGL `9.3.0`, adapter `0.5.2`, dependency lock |
| Firmware build/link and image validation | `scripts/idf.sh build` exited 0; `build/deye_hmi.bin`; local `dist/deye-hmi-0.1.0/image_info.txt` validates project/version/hash |
| C core correctness and sanitizer checks | 304 checks, zero failures, CTest 1/1; `build-tests/Testing/Temporary/LastTest.log` |
| Actual HTTP adapter error scenarios | 13 pytest tests; `backend/mock/tests/test_server.py` calls `tests/integration/telemetry_probe.c`; SOC change, alarm, upstream stale data, missing/malformed fields, timeout/offline covered |
| UI navigation and touch-event regressions | Actual LVGL harness, ten rendered screens; repeated Ready submission, PIN navigation/expiry and protected captures passed; `build-ui/screens/`, `tests/ui/ui_smoke.c` |
| History/memory bounds | Fixed rings, aggregation and multi-day ring-wrap tests in `tests/unit/test_history.c`; no claim of a completed hardware soak |
| Control lifecycle and uncertainty | Unsupported/current/range/freshness/cooldown/confirmation/dedup/read-back/uncertain-timeout/correlation/audit tests in `test_control.c`; writes remain disabled |
| Static analysis and formatting | `scripts/format.sh --check`, `scripts/static_analysis.sh`, Python ruff, `shellcheck scripts/*.sh`, `actionlint .github/workflows/ci.yml` passed |
| Physical-board evidence | Verified flash hashes and boot logs; display/GT911 init, automatic Wi-Fi/DHCP, NVS reload and real telemetry; five PNGs in `docs/screenshots/` |
| Screenshot access and locking | Actual protected Controls request returned 503; monitoring capture and display `/health`/`/metrics` passed; diagram in `docs/diagrams/screenshot-flow.md` |
| Credential and publication boundary | 152 tracked files checked against real local SSID/password and serial after the build corrections; private profile, sdkconfig, caches, managed dependencies and provisioned build/dist artifacts excluded |
| License compliance | Apache-2.0 `LICENSE`, `NOTICE`, vendor attribution; release packaging includes project and dependency license files |
| GitHub CI and downloadable artifacts | Run `44744552799` for `b5d2f5a` passed all four jobs, strict lock comparison, packaging/image validation and upload; artifact `14632149750` (`deye-hmi-firmware`), 4,101,994 bytes |
| Final report and honest categories | `docs/engineering-report.md` has all fourteen required sections, including exact build/flash commands, API mappings, binary locations, tests, limitations, hardware and control status |

The runtime source was built/flashed and captured before the audit updates.
The SDK correction changes dependency discovery, not application behavior.
Existing passing runtime checks are reused; these notes do not retrospectively
turn unperformed physical tests into passes.

## Clean-build regression

The unpatched ESP-IDF 5.5.1 configure passes newly registered managed components
back to Component Manager 2.3.0 as local roots on a Kconfig-triggered rerun.
A clean reproduction changed 16 registry dependencies into 25 local/registry
entries, including pinned local LVGL 9.3.0 and registry LVGL 9.6.0. The latter
overwrote the headers and failed the unchanged compiler warning checks.

`patches/esp-idf-managed-components.patch` filters only
`COMPONENT_SOURCE=project_managed_components` from that local-root list.
The same clean configure with this filter keeps 16 registry dependencies and
LVGL 9.3.0 across both passes. The project Dockerfile applies the small SDK
correction, and both local helpers and CI build that identical image.
No dependency version/hash or compiler warning policy was relaxed.

The lock file retains the manager's canonical serialization, including its
generated whitespace, so CI can require a byte-for-byte unchanged lock after
configuration and compilation.

The second CI run completed firmware compilation but detected hash-scalar
wrapping differences between ARM64 and AMD64 YAML emitters. The local SDK
reports `WITH_LIBYAML=False`. `component-manager-yaml.patch` explicitly selects
the Python emitter on both architectures; no lock value or CI comparison is
relaxed. Cross-platform CI run `44744552799` passed, including the unchanged
byte-for-byte lock gate and release upload.

## Explicitly unresolved acceptance checks

The missing write API, per-parameter firmware/register/persistence verification,
grid direction convention, generator/BMS fields and historical backend remain
external dependencies. Physical touch accuracy, flicker/tearing inspection,
manual provisioning, network fault recovery on hardware, screen wake,
static IPv4, OTA/rollback, factory reset and long-running heap stability remain
hardware-unverified. No inverter writes, automated replay, register discovery
writes or Raspberry Pi changes are part of this delivery.

## Deferred review findings

The final review found PIN-dialog expiry, endpoint-change isolation and
non-deterministic release packaging issues. They remain open in
`docs/backlog.md`. The latest delivery scope defers those fixes; closing this
delivery is not a claim that the findings or production-acceptance gate passed.
No runtime code was changed after the reviewed/CI-tested source revision.
