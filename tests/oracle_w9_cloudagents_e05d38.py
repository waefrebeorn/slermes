#!/usr/bin/env python3
"""Differential oracle for hermes_cli/cloud_agents.py (w9_cloudagents_e05d38 gate).

The network/auth edges of discover() are stubbed (the C port takes the HTTP
status/body as a fixture seam); the live module's own _org/_agent/discover code
runs unmodified for every case.
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


# Parent-package bootstrap for the live module's lazy imports.
cli = _mod("hermes_cli")
cli.__path__ = []
_mod("hermes_cli.auth",
     AuthError=type("AuthError", (Exception,), {}),
     get_provider_auth_state=lambda name: {},
     resolve_nous_access_token=lambda: "tok")
_mod("hermes_cli.auth_nous", _nous_portal_base_url=lambda state: "https://portal.invalid")

CURRENT = {}


class HTTPError(Exception):
    pass


class FakeResponse:
    def __init__(self, status_code, body):
        self.status_code = status_code
        self._body = body

    def json(self):
        return self._body


class FakeClient:
    def __init__(self, **kwargs):
        pass

    def __enter__(self):
        return self

    def __exit__(self, *args):
        return False

    def get(self, url, params=None, headers=None):
        return FakeResponse(CURRENT["status"], CURRENT["body"])


_mod("httpx", Client=FakeClient, HTTPError=HTTPError)

SRC = Q + "/hermes_cli/cloud_agents.py"
spec = importlib.util.spec_from_file_location("w9cloud_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w9cloud_live"] = live
spec.loader.exec_module(live)

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func = r.get("func")
    try:
        if func == "org":
            py = live._org(r.get("raw"))
        elif func == "agent":
            py = live._agent(r.get("raw"))
        elif func == "status":
            py = live.PORTAL_SIGNIN_STATUS
        elif func == "response":
            CURRENT["status"] = r.get("status_code", 200)
            CURRENT["body"] = r.get("body")
            try:
                py = live.discover(org=r.get("org"))
            except live.CloudAgentsError as exc:
                py = {"status": exc.status, "code": exc.code, "message": str(exc)}
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w9_cloudagents_e05d38: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
