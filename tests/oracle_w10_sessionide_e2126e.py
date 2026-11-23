#!/usr/bin/env python3
"""Differential oracle for gate w10_sessionide_e2126e (gateway/session_identity.py)."""
import importlib.util, json, os, sys, types
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _pkg(name):
    m = sys.modules.get(name)
    if m is None:
        m = types.ModuleType(name)
        m.__path__ = [os.path.join(Q, *name.split("."))]
        sys.modules[name] = m
    return m


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


# is_authenticated_identity imports these constants inside the function.
_pkg("hermes_cli")
_pkg("hermes_cli.dashboard_auth")
_mod("hermes_cli.dashboard_auth.ws_tickets",
     INTERNAL_PROVIDER="server-internal", INTERNAL_USER_ID="server-internal")

NAME = "w10_sessionide_e2126e_live"
sp = importlib.util.spec_from_file_location(NAME, os.path.join(Q, "gateway", "session_identity.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)


def norm(v):
    if isinstance(v, (list, tuple)):
        return [norm(x) for x in v]
    if isinstance(v, dict):
        return {k: norm(x) for k, x in v.items()}
    return v


def compute(r):
    f = r.get("func")
    if f == "name":
        return live._name(r.get("value"))
    if f == "parse_subject":
        return norm(live.parse_authenticated_subject(r.get("subject")))
    if f == "authenticated_subject":
        return live.authenticated_subject(r.get("identity") or {})
    if f == "is_authenticated_identity":
        return live.is_authenticated_identity(r.get("identity"))
    return None


n = m = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if not isinstance(r, dict) or "out" not in r:
        continue
    n += 1
    py, got = compute(r), r.get("out")
    if py != got:
        m += 1
        print("MISMATCH %s py=%r c=%r" % (r.get("func"), py, got), file=sys.stderr)
print("ORACLE w10_sessionide_e2126e: %d cases, %d mismatches" % (n, m))
sys.exit(1 if m else 0)
