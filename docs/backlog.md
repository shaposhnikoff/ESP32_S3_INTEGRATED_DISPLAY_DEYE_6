# Deferred work

These findings remain open. On 2026-11-20 the current delivery was closed
with these items deferred for later work; no fixes are claimed.
See [independent-review.md](independent-review.md).

- [ ] **High: revoke protected dialogs when the PIN session expires.**
  Source: `components/ui/ui_main.c:238`, `ui_widgets.c:268`,
  `scr_system.c:110`. Expiry changes the underlying page but leaves an open
  dialog and its callback usable. Protected settings or maintenance actions
  can still be submitted after expiry.
  Recheck authorization before protected actions and safely cancel pending
  protected dialogs/callbacks. Add open-text-dialog and open-OTA-confirmation
  expiry regressions.

- [ ] **High: isolate telemetry and history when the backend endpoint changes.**
  Source: `components/app/app_state.c:111`, `telemetry.c:134`,
  `components/ui/ui_main.c:72`. A request started before changing host/port
  can publish the previous endpoint's data as fresh; existing history is
  retained across endpoints.
  Version endpoint configuration, invalidate endpoint-specific state/history,
  and discard results from older generations. Test a configuration change
  during an in-flight fetch.

- [ ] **Medium: make release archives byte-reproducible.**
  Source: `scripts/package_release.sh:44`. Current wall-clock build metadata,
  archive timestamps/ownership/order and reuse of the output directory
  prevent deterministic repackaging.
  Use `SOURCE_DATE_EPOCH` or commit time, normalize archive metadata/order,
  and build from a fresh generated output directory. Package identical inputs
  twice and compare archive digests.

The first two findings were confirmed by source inspection; targeted runtime
regressions have not been added. Existing passing tests do not cover them.
The third concerns package byte identity; the pinned firmware build, strict
dependency lock gate and image validation already pass CI.
