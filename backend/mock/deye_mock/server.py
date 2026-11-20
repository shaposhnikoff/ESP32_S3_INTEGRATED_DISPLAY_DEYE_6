"""Mock of the Deye Prometheus exporter (GET /metrics, GET /health).

The baseline payload is the real capture in tests/fixtures. Scenarios
mutate it to simulate conditions that are hard to reproduce on the real
inverter. The mock is read-only: it exposes no write endpoints because the
real backend has none (see docs/api-contract.md).
"""

from __future__ import annotations

import argparse
import json
import re
import threading
import time
from dataclasses import dataclass, field
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlparse

FIXTURE = Path(__file__).resolve().parents[3] / "tests" / "fixtures" / "metrics_live_2026-11-20.txt"

SCENARIOS = (
    "normal",  # baseline capture, SOC drifts slowly
    "soc_drop",  # SOC 80 -> 75 over the first minute
    "alarm",  # running status alarm
    "backend_stale",  # exporter lost Modbus: scrape_success 0, large data age
    "offline",  # every request fails with 503
    "timeout",  # requests hang longer than the client timeout
    "malformed",  # truncated / garbage payload
    "missing_core",  # SOC and load power absent
)


def _set(text: str, metric: str, value: str) -> str:
    """Replace the value of every sample of ``metric`` (any labels)."""
    pattern = re.compile(rf"^({re.escape(metric)}(?:\{{[^}}]*\}})?) \S+$", re.MULTILINE)
    return pattern.sub(lambda m: f"{m.group(1)} {value}", text)


def _drop(text: str, metric: str) -> str:
    return "\n".join(line for line in text.splitlines() if not line.startswith(metric))


@dataclass
class MockState:
    """Mutable scenario state shared by request handler threads."""

    scenario: str = "normal"
    started: float = field(default_factory=time.monotonic)
    requests: int = 0
    lock: threading.Lock = field(default_factory=threading.Lock)

    def set_scenario(self, name: str) -> None:
        """Switch scenario and restart its clock."""
        if name not in SCENARIOS:
            raise ValueError(f"unknown scenario {name!r}")
        with self.lock:
            self.scenario = name
            self.started = time.monotonic()


def render_metrics(base: str, scenario: str, elapsed_s: float, now_unix: float) -> str:
    """Return the exporter payload for ``scenario`` at ``elapsed_s`` seconds into it."""
    text = _set(base, "deye_last_successful_scrape_timestamp_seconds", f"{int(now_unix)}")
    text = _set(text, "deye_data_age_seconds", "1.0")
    if scenario == "normal":
        soc = max(10.0, 94.0 - elapsed_s / 600.0)
        text = _set(text, "deye_battery_percent", f"{soc:.1f}")
    elif scenario == "soc_drop":
        soc = max(75.0, 80.0 - 5.0 * min(elapsed_s, 60.0) / 60.0)
        text = _set(text, "deye_battery_percent", f"{soc:.1f}")
    elif scenario == "alarm":
        text = text.replace(
            'status="normal",serial="TEST000001"} 1', 'status="normal",serial="TEST000001"} 0'
        )
        text = text.replace('status="alarm",serial="TEST000001"} 0', 'status="alarm",serial="TEST000001"} 1')
    elif scenario == "backend_stale":
        text = _set(text, "deye_scrape_success", "0")
        text = _set(text, "deye_data_age_seconds", f"{600.0 + elapsed_s:.1f}")
    elif scenario == "malformed":
        text = text[: len(text) // 3] + '\ndeye_garbage{unterminated="x 1\n\x00\xff'
    elif scenario == "missing_core":
        text = _drop(_drop(text, "deye_battery_percent"), "deye_load_power_watts")
    return text


def make_handler(state: MockState, base: str, hang_s: float) -> type[BaseHTTPRequestHandler]:
    """Build a request handler class bound to ``state``."""

    class Handler(BaseHTTPRequestHandler):
        server_version = "deye-mock/0.1"

        def log_message(self, fmt: str, *args: object) -> None:  # noqa: D102
            return

        def _send(self, code: int, body: bytes, ctype: str) -> None:
            self.send_response(code)
            self.send_header("Content-Type", ctype)
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def do_GET(self) -> None:  # noqa: N802
            url = urlparse(self.path)
            with state.lock:
                state.requests += 1
                scenario = state.scenario
                elapsed = time.monotonic() - state.started
            if url.path == "/_scenario":
                name = parse_qs(url.query).get("name", [""])[0]
                try:
                    state.set_scenario(name)
                except ValueError as exc:
                    self._send(400, str(exc).encode(), "text/plain")
                    return
                self._send(200, json.dumps({"scenario": name}).encode(), "application/json")
                return
            if scenario == "offline":
                self._send(503, b"backend unavailable", "text/plain")
                return
            if scenario == "timeout":
                time.sleep(hang_s)
            if url.path == "/metrics":
                body = render_metrics(base, scenario, elapsed, time.time()).encode("utf-8", "replace")
                self._send(200, body, "text/plain; version=0.0.4; charset=utf-8")
            elif url.path == "/health":
                ok = scenario != "backend_stale"
                payload = {
                    "ok": ok,
                    "serial": "TEST000001",
                    "age_seconds": 1.0 if ok else 600.0,
                    "last_success": time.time(),
                    "error": None if ok else "modbus timeout",
                }
                self._send(200, json.dumps(payload).encode(), "application/json")
            else:
                self._send(404, b"not found", "text/plain")

    return Handler


def build_server(
    host: str, port: int, scenario: str = "normal", hang_s: float = 15.0
) -> tuple[ThreadingHTTPServer, MockState]:
    """Create (but do not start) a mock server."""
    state = MockState()
    state.set_scenario(scenario)
    base = FIXTURE.read_text(encoding="utf-8")
    server = ThreadingHTTPServer((host, port), make_handler(state, base, hang_s))
    return server, state


def main() -> None:
    """CLI entry point."""
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--host", default="0.0.0.0")
    ap.add_argument("--port", type=int, default=9101)
    ap.add_argument("--scenario", default="normal", choices=SCENARIOS)
    args = ap.parse_args()
    server, _ = build_server(args.host, args.port, args.scenario)
    print(f"deye-mock on http://{args.host}:{args.port} scenario={args.scenario}")
    print("switch scenario: GET /_scenario?name=<" + "|".join(SCENARIOS) + ">")
    server.serve_forever()


if __name__ == "__main__":
    main()
