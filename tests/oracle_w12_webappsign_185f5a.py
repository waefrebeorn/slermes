#!/usr/bin/env python3
"""Differential oracle for upstream Hermes hermes_cli/webapp_sign_in.py (gate w12_webappsign_185f5a).

Heavy top-level imports are stubbed in sys.modules before exec_module; only the
pure helpers binding/_digest/applies are driven.  HMAC-SHA256 and SHA256 are real
stdlib, so the C port is compared against the genuine hash primitives.
"""
import importlib.util
import json
import sys
import types

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")


def _pkg(name):
    m = types.ModuleType(name)
    m.__path__ = []
    sys.modules[name] = m
    return m


# stub fastapi (top-level) and hermes_cli.browser_credential
fastapi = _pkg("fastapi")
fastapi.Request = type("Request", (), {})
resp = _pkg("fastapi.responses")
resp.Response = type("Response", (), {})
fastapi.responses = resp
hcli = _pkg("hermes_cli")
bc = _pkg("hermes_cli.browser_credential")
bc.current = lambda: "stub"
bc.generation_of = lambda token: 0
bc.current_if = lambda gen: "stub"
hcli.browser_credential = bc

SRC = QUARRY + "/hermes_cli/webapp_sign_in.py"
spec = importlib.util.spec_from_file_location("webapp_sign_in_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["webapp_sign_in_live"] = live
spec.loader.exec_module(live)


def compute(r):
    func = r.get("func")
    if func == "binding":
        return live.binding(r.get("token", ""))
    if func == "digest":
        return live._digest(r.get("token", ""))
    if func == "applies":
        state = types.SimpleNamespace(
            ui_surface=r.get("ui_surface"),
            auth_required=r.get("auth_required", False),
        )
        req = types.SimpleNamespace(app=types.SimpleNamespace(state=state))
        return bool(live.applies(req))
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    py = compute(r)
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {r.get('func')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w12_webappsign_185f5a: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
