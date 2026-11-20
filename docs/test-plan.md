# Test plan

## Layers

| Layer | Tool | Runs where | Command |
|---|---|---|---|
| Core unit tests (parser, model, freshness, history, control, formatting) | C harness + ASan/UBSan | host (macOS clang, Linux gcc) | `scripts/host_tests.sh` |
| Mock backend tests | pytest | host | `cd backend/mock && uv run pytest` |
| Static analysis | cppcheck 2.13, clang-format 19.1.7, ruff, shellcheck, actionlint | host / CI | `scripts/static_analysis.sh`, `scripts/format.sh --check` |
| Firmware build + size + artifact validation | ESP-IDF 5.5.1 container | host / CI | `scripts/idf.sh build && scripts/package_release.sh` |
| HTTP integration with mock backend | Python HTTP client + real C parser bridge | host, passed | `scripts/host_tests.sh` |
| UI navigation and regressions | real LVGL memory display + ASan/UBSan | host, passed | test-results.md |
| Hardware validation | boot logs, actual device capture and manual checks | partially passed | hardware-validation.md |

## Scenario coverage

| Plan scenario | Covered by |
|---|---|
| SOC 80 -> 75 % | `metrics_soc75.txt` test; mock `soc_drop` |
| Grid disappears | live fixture (grid disconnected); Grid screen state |
| Backend unavailable | `link_state` offline tests; mock `offline` |
| Backend timeout | backoff tests; mock `timeout` |
| Inverter alarm | `test_alarm`; mock `alarm` |
| Exporter lost Modbus | `test_backend_poll_failed_is_stale`; mock `backend_stale` |
| Missing / invalid fields | `test_missing_core`, `test_invalid_values`; mock `missing_core`, `malformed` |
| Unsupported parameter | `test_shipped_defaults_block_all_writes`, `CTRL_ERR_UNSUPPORTED` |
| Write timeout / Wi-Fi drop during write | `test_timeout_is_uncertain_and_never_replayed` |
| Accepted but read-back disagrees | `test_readback_disagrees` |
| Duplicate commands / double tap | `test_happy_path_requires_readback` (BUSY, duplicate confirm) |
| Reboot with pending command | by construction (RAM-only control context) - not testable on host |
| Bounded history / long run | `test_ring_wraps_and_bounds` (3 days of samples) |

## Pending (needs hardware)

Physical touch accuracy, Wi-Fi reconnect, fault recovery under telemetry,
24 h soak with heap watermark, OTA + rollback.
Display initialization, live device captures, file-configured network
connection and NVS reload have passed on the board.
Unsafe writes are never executed automatically; there is no write path.
