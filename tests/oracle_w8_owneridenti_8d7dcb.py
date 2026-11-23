#!/usr/bin/env python3
"""Differential oracle for upstream Hermes gateway/owner_identity.py
(gate w8_owneridenti_8d7dcb).

host_owner_subject / list_session_providers / owner_rule are injected seams;
the real session_identity and session_rules.legacy_local algorithms are used.
"""
import importlib.util
import json
import sys
import types
from types import SimpleNamespace

for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m

def _ensure_pkg(name, path):
    if name not in sys.modules:
        m = types.ModuleType(name)
        m.__path__ = [path]
        sys.modules[name] = m
    return sys.modules[name]

import gateway.session_identity            # real auth-subject algorithms
import session_rules.legacy_local           # real legacy owner rule

EXTRA = []
RULE = {}
class _Provider:
    def __init__(self, name):
        self.name = name

_ensure_pkg("hermes_cli", Q + "/hermes_cli")
_ensure_pkg("hermes_cli.dashboard_auth", Q + "/hermes_cli/dashboard_auth")
_mod("hermes_cli.dashboard_auth.registry",
     list_session_providers=lambda: [_Provider(n) for n in EXTRA])

def _owner_rule():
    return SimpleNamespace(host_subject=RULE.get("host_subject", ""),
                           login_providers=frozenset(RULE.get("login_providers", [])),
                           messaging_labels=frozenset())
_mod("gateway.session_persistence", owner_rule=_owner_rule)

spec = importlib.util.spec_from_file_location("owner_identity_live", Q + "/gateway/owner_identity.py")
live = importlib.util.module_from_spec(spec)
sys.modules["owner_identity_live"] = live
spec.loader.exec_module(live)

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    func = r.get("func")
    EXTRA[:] = r.get("extra") or []
    RULE.clear()
    RULE.update(r.get("rule") or {})
    try:
        if func == "login_provider":
            py = live._login_provider(r.get("provider"))
        elif func == "principal":
            live.host_owner_subject = lambda: r.get("host", "")
            py = live.principal_subject(r.get("identity") or {})
        elif func == "same_owner":
            py = live.same_owner(r.get("stored"), r.get("subject"))
        elif func == "owners_agree":
            py = live.owners_agree(r.get("first"), r.get("second"))
        else:
            continue
    except Exception as exc:
        print("LIVE ERROR %s %r: %r" % (func, r, exc), file=sys.stderr)
        mism += 1
        cases += 1
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (func, r.get("case"), py, got), file=sys.stderr)

print("ORACLE w8_owneridenti_8d7dcb: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
