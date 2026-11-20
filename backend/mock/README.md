# deye-mock

Read-only mock of the Deye Prometheus exporter used by the HMI.

```bash
cd backend/mock
uv run deye-mock --port 9101 --scenario normal
curl "http://localhost:9101/_scenario?name=offline"   # switch scenario at runtime
uv run pytest && uv run ruff check . && uv run ruff format --check .
```

Scenarios: `normal`, `soc_drop`, `alarm`, `backend_stale`, `offline`, `timeout`, `malformed`, `missing_core`.
Point the HMI backend host at the machine running the mock.
