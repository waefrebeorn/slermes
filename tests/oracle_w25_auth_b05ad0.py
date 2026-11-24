#!/usr/bin/env python3
"""Differential oracle for plugins/model-providers/solstice/auth.py
(gate w25_auth_b05ad0).

The live module is loaded from its hyphenated plugin path.  Every NAS call is
answered from the fixture: ``hermes_cli.auth_constants.httpx`` is replaced by a
fake that records the exact (method, path, body) the live module composed and
then returns the fixture's status/payload (or raises).  So the request
composition, the 404 / >=400 -> AuthError mapping, the missing-config
validation and the refresh -> forced re-login fold all run for real; only the
network, the user's Nous token and the TLS policy are fixed inputs.

Response shape per case (what the C port must produce):
    {"ok": bool, "value": <payload|object|null>, "code": <str|null>,
     "relogin": bool, "request": {"method","path","body"}|null}
"""
import importlib.util
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w25_auth_b05ad0"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

import hermes_cli.auth as ha  # noqa: E402
import hermes_cli.auth_constants as hc  # noqa: E402

_spec = importlib.util.spec_from_file_location(
    "solstice_auth_live", QUARRY / "plugins" / "model-providers" / "solstice" / "auth.py")
live = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(live)

WANTED = {"discover", "broker", "constants"}
BASE = "https://nas.solstice.oracle.invalid"

CONSTANTS = (
    ("nas_base_url", (live, "DEFAULT_NAS_BASE_URL")),
    ("config_path", (live, "_CONFIG_PATH")),
    ("timeout_seconds", (live, "_TIMEOUT_SECONDS")),
    ("token_paths", (live, "_TOKEN_PATHS")),
)


class _Resp:
    def __init__(self, status, payload):
        self.status_code = status
        self._payload = payload

    def json(self):
        return self._payload


def _error_type(name):
    return type(str(name), (Exception,), {})


class _HTTP:
    """Records what the live module composed, then answers from the fixture."""

    def __init__(self, nas, default_payload):
        self.nas = nas or {}
        self.default_payload = default_payload
        self.calls = []

    def request(self, method, url, json=None, timeout=None, headers=None, verify=None):
        path = url[len(BASE):] if url.startswith(BASE) else url
        self.calls.append({"method": method, "path": path, "body": json})
        raise_name = self.nas.get("raise")
        if raise_name:
            raise _error_type(raise_name)("oracle: transport failure")
        if "payload" in self.nas:
            payload = self.nas["payload"]
        else:
            payload = self.default_payload
        return _Resp(int(self.nas.get("status", 200)), payload)


def _token_getter(boom):
    def get():
        if boom:
            raise RuntimeError("oracle: no nous token")
        return "ORACLE-TOKEN"
    return get


def _envelope(fn, http):
    try:
        value = fn()
    except BaseException as exc:  # noqa: BLE001 - the port mirrors the failure class
        return {
            "ok": False,
            "value": None,
            "code": getattr(exc, "code", None) or type(exc).__name__,
            "relogin": bool(getattr(exc, "relogin_required", False)),
            "request": http.calls[-1] if http.calls else None,
        }
    return {
        "ok": True,
        "value": value,
        "code": None,
        "relogin": False,
        "request": http.calls[-1] if http.calls else None,
    }


def discover(req):
    http = _HTTP({}, req.get("payload"))
    _install(http, boom=False)
    return _envelope(live.discover_client, http)


def broker(req):
    grant = req.get("grant")
    nas = req.get("nas")
    http = _HTTP(nas, None)
    _install(http, boom=bool((nas or {}).get("token_error")))
    return _envelope(lambda: live.broker_token_request(grant), http)


def _install(http, boom):
    hc.httpx = http
    ha.resolve_nous_access_token = _token_getter(boom)
    ha._nous_portal_base_url = lambda state: BASE
    ha.get_provider_auth_state = lambda provider: {}
    ha._default_verify = lambda: True


def constants():
    out = {}
    for key, (owner, attr) in CONSTANTS:
        out[key] = getattr(owner, attr)
    return out


def expected(req):
    func = req.get("func")
    if func == "discover":
        return discover(req)
    if func == "broker":
        return broker(req)
    if func == "constants":
        return constants()
    raise AssertionError("unknown func %r" % (func,))


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            req = json.loads(line)
        except ValueError:
            continue
        if not isinstance(req, dict) or req.get("func") not in WANTED:
            continue
        cases += 1
        try:
            py = expected(req)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r"
                  % (req.get("func"), req.get("case"), exc), file=sys.stderr)
            continue
        got = req.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (req.get("func"), req.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
