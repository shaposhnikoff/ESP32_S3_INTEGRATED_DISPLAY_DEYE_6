"""Tests for the mock exporter: scenarios must produce what the firmware expects."""

from __future__ import annotations

import ctypes
import json
import threading
import urllib.error
import urllib.request
from collections.abc import Iterator

import pytest

from deye_mock.server import FIXTURE, SCENARIOS, build_server, render_metrics

BASE = FIXTURE.read_text(encoding="utf-8")


@pytest.fixture(scope="session")
def core_probe() -> ctypes.CDLL:
    """Load the real firmware core built by scripts/host_tests.sh."""
    library = ctypes.CDLL(str(FIXTURE.parents[2] / "build-tests/libtelemetry_probe.so"))
    library.telemetry_probe.argtypes = [ctypes.c_char_p, ctypes.c_size_t]
    library.telemetry_probe.restype = ctypes.c_char_p
    return library


def _value(text: str, name: str) -> float:
    for line in text.splitlines():
        if line.startswith(name + "{") or line.startswith(name + " "):
            return float(line.rsplit(" ", 1)[1])
    raise KeyError(name)


@pytest.fixture
def server() -> Iterator[tuple[str, object]]:
    srv, state = build_server("127.0.0.1", 0, hang_s=0.5)
    thread = threading.Thread(target=srv.serve_forever, daemon=True)
    thread.start()
    try:
        yield f"http://127.0.0.1:{srv.server_address[1]}", state
    finally:
        srv.shutdown()
        srv.server_close()
        thread.join(timeout=5)


def _get(url: str, timeout: float = 5.0) -> tuple[int, str]:
    try:
        with urllib.request.urlopen(url, timeout=timeout) as resp:
            return resp.status, resp.read().decode("utf-8", "replace")
    except urllib.error.HTTPError as err:
        return err.code, err.read().decode()


def test_normal_keeps_live_values() -> None:
    text = render_metrics(BASE, "normal", 0.0, 1_800_000_000)
    assert _value(text, "deye_battery_percent") == pytest.approx(94.0)
    assert _value(text, "deye_load_power_watts") == pytest.approx(729.0)
    assert _value(text, "deye_last_successful_scrape_timestamp_seconds") == 1_800_000_000


def test_soc_drop_80_to_75() -> None:
    assert _value(render_metrics(BASE, "soc_drop", 0.0, 0), "deye_battery_percent") == pytest.approx(80.0)
    assert _value(render_metrics(BASE, "soc_drop", 120.0, 0), "deye_battery_percent") == pytest.approx(75.0)


def test_alarm_is_one_hot() -> None:
    text = render_metrics(BASE, "alarm", 0.0, 0)
    assert 'status="alarm",serial="TEST000001"} 1' in text
    assert 'status="normal",serial="TEST000001"} 0' in text


def test_backend_stale() -> None:
    text = render_metrics(BASE, "backend_stale", 10.0, 0)
    assert _value(text, "deye_scrape_success") == 0
    assert _value(text, "deye_data_age_seconds") == pytest.approx(610.0)


def test_missing_core_drops_only_core() -> None:
    text = render_metrics(BASE, "missing_core", 0.0, 0)
    with pytest.raises(KeyError):
        _value(text, "deye_battery_percent")
    with pytest.raises(KeyError):
        _value(text, "deye_load_power_watts")
    assert _value(text, "deye_load_l1_power_watts") == pytest.approx(729.0)


def test_http_endpoints_and_scenario_switch(server: tuple[str, object]) -> None:
    url, _state = server
    status, body = _get(url + "/metrics")
    assert status == 200 and "deye_battery_percent" in body
    status, body = _get(url + "/health")
    assert status == 200 and json.loads(body)["ok"] is True
    assert _get(url + "/_scenario?name=offline")[0] == 200
    assert _get(url + "/metrics")[0] == 503
    assert _get(url + "/_scenario?name=bogus")[0] == 400
    assert _get(url + "/nope")[0] == 503  # offline applies to every path


def test_timeout_scenario_hangs(server: tuple[str, object]) -> None:
    url, _state = server
    _get(url + "/_scenario?name=timeout")
    with pytest.raises((TimeoutError, urllib.error.URLError)):
        _get(url + "/metrics", timeout=0.1)


def test_all_scenarios_render() -> None:
    for name in SCENARIOS:
        assert isinstance(render_metrics(BASE, name, 5.0, 0), str)


@pytest.mark.parametrize(
    ("scenario", "accepted", "soc", "load", "status", "fresh"),
    [
        ("normal", True, 94.0, 729.0, 3, True),
        ("soc_drop", True, 80.0, 729.0, 3, True),
        ("alarm", True, 94.0, 729.0, 4, True),
        ("backend_stale", True, 94.0, 729.0, 3, False),
        ("missing_core", False, 0.0, 0.0, 3, False),
    ],
)
def test_http_payload_reaches_firmware_model(
    server: tuple[str, object],
    core_probe: ctypes.CDLL,
    scenario: str,
    accepted: bool,
    soc: float,
    load: float,
    status: int,
    fresh: bool,
) -> None:
    """Exercise the mock's wire response with the actual C parser and freshness core."""
    url, _state = server
    assert _get(url + "/_scenario?name=" + scenario)[0] == 200
    code, body = _get(url + "/metrics")
    assert code == 200
    payload = body.encode()
    model = json.loads(core_probe.telemetry_probe(payload, len(payload)))
    assert model["accepted"] is accepted
    assert model["soc"] == pytest.approx(soc, abs=0.1)
    assert model["status"] == status
    assert model["fresh"] is fresh
    assert model["load"] == pytest.approx(load, abs=0.1)
