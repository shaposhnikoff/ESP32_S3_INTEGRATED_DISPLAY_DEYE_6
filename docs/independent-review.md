# Independent source review

Date: 2026-11-20. Code/build revision: `b5d2f5a`.
Result: **REJECT: three open findings**.

A direct, stateless model review assessed 92 supplied, numbered source files:
application, board, portable core, ten-screen UI, tests, CI, build/release
scripts, SDK patches and selected contracts/reports. This was a separate
review, not the author's self-review. Earlier agent-based attempts stalled
or failed before returning a source assessment.

The review identified two high-severity findings and one medium-severity
finding. The findings, source references, required corrections and regression
criteria are preserved as unchecked tasks in [backlog.md](backlog.md).
They are deferred, not resolved. No runtime fixes were applied after review.

## Coverage and limits

The assessed code uses GET-only telemetry, no direct Modbus transport and no
inverter write transport. All candidate parameters remain unverified.
Freshness incorporates exporter age and poll success; receive/history storage
is bounded. Screenshot capture uses the display lock and releases it before
transmission.

The reviewer accepted existing evidence rather than rerunning it: 304
sanitized C checks, 13 HTTP/C integration tests, the ten-screen LVGL harness,
physical-board captures and successful CI run `44744552799`. Those tests do
not establish coverage of the newly identified PIN-dialog and endpoint-change
interleavings.

Root build configuration, Kconfig, dependency lock, fixture contents and other
unsupplied documentation were not independently assessed in this direct
review. Build/lock/image gates are separately evidenced by the successful CI.

Missing write API, generator/BMS fields and backend history remain documented
external dependencies. Physical touch acceptance, OTA/rollback, hardware
fault recovery, wake/reset behavior and 24-hour stability remain unverified.
This delivery is a monitoring prototype with known issues, not production
acceptance.
