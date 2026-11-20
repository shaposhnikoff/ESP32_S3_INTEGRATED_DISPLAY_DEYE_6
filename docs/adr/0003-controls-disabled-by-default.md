# ADR 0003: Control safety layer shipped with all writes disabled

Status: accepted (2026-11-20)

Decision: implement and test the full command lifecycle (validation,
confirmation, cooldown, single in-flight command, read-back, UNCERTAIN on
timeout, audit), but ship with every parameter `verified = false` and
`backend_write_api = false`. Enabling is a code change per parameter after
the procedure in writable-parameters.md.
