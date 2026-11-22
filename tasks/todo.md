# 7-inch outage dial overview

- [x] Implement bounded schedule model, geometry, countdown, coverage and status rules in `deye_core`.
- [x] Replace Overview with a 24-hour clock dial and preserve live telemetry.
- [x] Support the Waveshare 7B and construct only Overview to retain internal RAM.
- [x] Fetch and validate confirmed group 4.1 outages from the public Kyiv-region JSON.
- [x] Run core and UI tests, build and flash firmware, inspect a live 7B screenshot.
- [x] Update runtime diagrams and document the production schedule source.

## Review

Physical verification on 2026-11-22: HTTPS returned 200 with 62,222 bytes;
today's plan contained two intervals. After a screenshot, uptime was 60 s,
internal heap free 35.6 KB, and minimum internal heap free 22.7 KB. The
screen showed the 24-hour dial, white hand and live telemetry. Long-running
stability and the next scheduled 10-minute refresh remain unverified.
