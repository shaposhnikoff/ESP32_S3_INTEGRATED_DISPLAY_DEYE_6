# ADR 0004: Bounded volatile on-device history

Status: accepted (2026-11-20)

Context: exporter metrics are instantaneous; no Prometheus server is present.

Decision: keep 1-minute means for 24 h and 15-minute means for 7 days in a
fixed ~68 KB PSRAM structure; lost on reboot; charts state this explicitly.
A Prometheus `query_range` source can replace it later behind `hist_query`.
