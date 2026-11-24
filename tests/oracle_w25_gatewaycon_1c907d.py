#!/usr/bin/env python3
"""Differential oracle for hermes_cli/gateway_connections_probe.py (gate w25_gatewaycon_1c907d).

The live probe runs for real; its seams are fixed answers -- one registry read
(the connect entry, its URL, token, headers and OAuth set), the HTTP client and
its error type.  What is compared is the decision those answers make: which
credential header the entry yields, and what one /api/status answer means.
"""
import asyncio
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w25_gatewaycon_1c907d"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from hermes_cli import gateway_connections as store  # noqa: E402
from hermes_cli import gateway_connections_probe as live  # noqa: E402

WANTED = {"credentials", "probe", "constants"}

# The HTTP seam: _probe imports httpx and config_env's client factory.
CASE = {}


class ProbeHTTPError(Exception):
    """Stand-in for httpx.HTTPError (its str() is the reason text)."""


class _Response:
    def __init__(self, req):
        self.status_code = req.get("status")

    def json(self):
        if CASE.get("json_error"):
            raise ValueError("no json")
        return CASE.get("body")


class _Client:
    async def __aenter__(self):
        return self

    async def __aexit__(self, *args):
        return False

    async def get(self, url, headers=None):
        if CASE.get("net_error"):
            raise ProbeHTTPError(CASE["net_error"])
        return _Response(CASE)


def _probe_client(base_url, timeout):
    return _Client()


fake_httpx = types.ModuleType("httpx")
fake_httpx.HTTPError = ProbeHTTPError
sys.modules["httpx"] = fake_httpx
fake_config_env = types.ModuleType("hermes_cli.web_routers.config_env")
fake_config_env._endpoint_probe_client = _probe_client
sys.modules["hermes_cli.web_routers.config_env"] = fake_config_env


def credentials(req):
    snapshot = json.loads(json.dumps(req.get("snapshot")))
    saved = store.dial_snapshot
    store.dial_snapshot = lambda connection_id: snapshot
    try:
        url, headers = live._credential_headers(req.get("connection_id") or "conn-1")
        return {"url": url, "headers": headers}
    except store.ConnectionStoreError as exc:
        return {"error": str(exc)}
    finally:
        store.dial_snapshot = saved


def probe(req):
    CASE.clear()
    CASE.update(req)
    try:
        return asyncio.run(live._probe(req.get("base_url"), req.get("headers") or {}))
    except store.ConnectionStoreError as exc:
        return {"error": str(exc)}
    finally:
        CASE.clear()


def constants():
    return {"timeout_s": live._TIMEOUT_S}


def expected(r):
    f = r.get("func")
    if f == "credentials":
        return credentials(r)
    if f == "probe":
        return probe(r)
    if f == "constants":
        return constants()
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in WANTED:
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r"
                  % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
