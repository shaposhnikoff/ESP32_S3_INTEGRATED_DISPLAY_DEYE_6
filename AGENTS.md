# AGENTS.md - Deye HMI

Project rules for coding agents. Global rules still apply.

## Hard safety rules

* Never write to the inverter, never add an automatic or retried write path,
  never flip `verified` in `components/deye_core/src/control.c` without the
  procedure in `docs/writable-parameters.md` and explicit human approval.
* The display must never access Modbus/RS485. Backend access is HTTP GET only
  until a write API is documented.
* Do not flash hardware or deploy to the Raspberry Pi (192.168.10.39) without
  explicit authorization. Reading `GET /metrics` and `/health` is allowed.
* Unknown/stale data must never render as zero or as live.

## Architecture conventions

* Platform-independent logic goes in `components/deye_core` with host tests.
* All `lv_*` calls run in the LVGL task (timers/event callbacks) or under
  `esp_lv_adapter_lock()`. No blocking network calls in the LVGL task.
* Shared state lives in `app_state.c`; take the state lock and settings lock
  separately, never nested.
* No dynamic allocation per poll; buffers are fixed and allocated at boot.

## Commands

```bash
scripts/host_tests.sh          # unit + mock tests
scripts/format.sh --check      # clang-format 19.1.7
scripts/static_analysis.sh     # cppcheck
scripts/idf.sh build           # firmware (Docker espressif/idf:v5.5.1)
scripts/package_release.sh
```

Update `docs/` and `docs/diagrams/` when control flow or contracts change.
