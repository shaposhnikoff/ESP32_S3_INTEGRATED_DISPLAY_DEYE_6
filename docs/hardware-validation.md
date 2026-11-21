# Hardware validation

Status: **partially validated on 2026-11-20**, firmware 0.1.0.
The ESP32-S3-Touch-LCD-5B was flashed through native USB-Serial/JTAG.
Boot logs confirm RGB display, GT911 initialization, saved NVS settings,
automatic Wi-Fi connection, DHCP and the first successful telemetry fetch.
Real on-device screenshots are in `docs/screenshots/`.

On 2026-11-21 the glanceable Overview build was flashed over native USB
serial using individual images, preserving NVS. The board booted, joined Wi-Fi
at 192.168.10.24 and fetched live telemetry. `/health` returned OK, `/metrics`
reported uptime, heap and PSRAM, and HTTP captures of Overview and Charts
succeeded. `docs/screenshots/overview.png` is the live Overview
capture. Touch, long-running stability, and the five alert states still need
physical validation; their UI paths passed the host LVGL smoke harness.

Checklist (record date, firmware version, result, notes):

| # | Check | Result |
|---|---|---|
| 1 | Boots, backlight on, Overview rendered without tearing/flicker | boot and Overview capture passed; physical tearing/flicker inspection pending |
| 2 | Touch coordinates match (corners and nav rail) | pending |
| 3 | Wi-Fi scan, password entry, connect, IP shown | automatic file-configured connection passed; manual touch entry pending |
| 4 | Live telemetry matches the configured exporter values | passed: fetch log and device screenshots show actual data |
| 5 | Unplug Pi / stop exporter: STALE then OFFLINE, UI responsive, recovery | pending |
| 6 | Wi-Fi AP off/on: reconnect with backoff | pending |
| 7 | Battery sign confirmed while charging (PV or grid) | pending |
| 8 | Grid sign confirmed with grid connected (import and export) | pending |
| 9 | Screen timeout and wake-by-touch (no button press on wake) | pending |
| 10 | Settings survive reboot (NVS) | passed: second boot logs load the saved network profile |
| 11 | 24 h soak: min free heap stable (Diagnostics) | pending |
| 12 | OTA from HTTPS URL, and rollback on a deliberately broken image | pending |
| 13 | Factory reset erases settings only | pending |

Inverter write validation is out of scope until a write API exists
(writable-parameters.md).
