# ADR 0002: Reuse the Prometheus exporter, no gateway

Status: accepted (2026-11-20)

Context: the exporter already provides every displayed value. No write API is
reachable or documented.

Decision: the HMI reads `GET /metrics` directly. No FastAPI gateway is added
now, because it would not remove the real blocker (unverified registers and
an unknown write API) and would add a second process on the Pi.

Consequences: no history from the backend (on-device history instead); no
authentication on reads. Revisit when a write API exists.
