#!/usr/bin/env python3
"""Differential oracle for gate w13_sessionaut_846c76 (gateway/session_automation.py)."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
QUARRY = "C:/Users/eman5/hermes-agent"
G = "w13_sessionaut_846c76"
sys.path.insert(0, QUARRY)
NS = types.SimpleNamespace


def _seed(name, **attrs):
    mod = types.ModuleType(name)
    mod.__path__ = []
    for k, v in attrs.items():
        setattr(mod, k, v)
    sys.modules[name] = mod
    parts = name.split(".")
    for i in range(1, len(parts)):
        parent = sys.modules.get(".".join(parts[:i]))
        if parent is not None and not hasattr(parent, parts[i]):
            setattr(parent, parts[i], mod)
    return mod


for _pkg in ("gateway", "gateway.platforms", "persistence", "persistence.core", "session_rules"):
    if _pkg not in sys.modules:
        _seed(_pkg)


class AuthorityRefusal(Exception):
    def __init__(self, reason):
        super().__init__(reason)
        self.reason = reason


_seed("gateway.config", Platform=NS(LOCAL="local", TELEGRAM="telegram", API_SERVER="api_server"))
_seed("gateway.platforms.event", MessageEvent=type("MessageEvent", (), {}), MessageType=NS(TEXT="text"))
_seed("gateway.session_contract", SessionRef=type("SessionRef", (), {}))
_seed("gateway.session_persistence", authority_persistence=lambda a: None)
_seed("persistence.core.errors", AuthorityRefusal=AuthorityRefusal)
_seed("session_rules.automation", automation_descriptor=lambda p: {})

SRC = QUARRY + "/gateway/session_automation.py"
_spec = importlib.util.spec_from_file_location(G + "_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules[G + "_live"] = live
_spec.loader.exec_module(live)


def canon(o):
    if type(o) is float and o.is_integer():
        return int(o)
    if isinstance(o, dict):
        return {k: canon(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [canon(v) for v in o]
    return o


def compute(r):
    func = r.get("func")
    if func == "identities":
        return live._identities(r.get("raws"))
    if func == "queued_tick":
        return live._queued_tick(NS(**(r.get("row") or {})), r.get("process"), r.get("route"))
    if func == "notification":
        try:
            return live.automation_notification_metadata(r.get("metadata") or {})
        except AuthorityRefusal as exc:
            return {"error": exc.reason}
    if func == "admission_display":
        return live.admission_display(r.get("payload") or {})
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line.startswith("{"):
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        py = {"__raise__": type(exc).__name__, "msg": str(exc)}
    got = r.get("out")
    if canon(py) != canon(got):
        mism += 1
        print(f"MISMATCH {G} case={r.get('case')} func={r.get('func')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
