#!/usr/bin/env python3
"""Differential oracle for gate w14_sessioncon_1a39cb (gateway/session_connections.py)."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
QUARRY = "C:/Users/eman5/hermes-agent"
G = "w14_sessioncon_1a39cb"
sys.path.insert(0, QUARRY)


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


for _pkg in ("persistence", "persistence.core"):
    if _pkg not in sys.modules:
        _seed(_pkg)


class AuthorityRefusal(Exception):
    def __init__(self, reason, *, field=None, detail=None):
        self.reason = reason
        self.field = field
        self.detail = detail
        super().__init__(reason)


_seed("persistence.core.errors", AuthorityRefusal=AuthorityRefusal)

SRC = QUARRY + "/gateway/session_connections.py"
_spec = importlib.util.spec_from_file_location(G + "_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules[G + "_live"] = live
_spec.loader.exec_module(live)


def compute(r):
    func = r.get("func")
    if func == "session_owned":
        return live.session_owned(r.get("params"))
    if func == "methods":
        return list(live.METHODS)
    if func == "connector_refusal":
        e = live.ConnectorRefusal(r.get("code"), r.get("reason"), r.get("message"))
        return {"wire_code": e.wire_code, "wire_message": e.wire_message,
                "reason": e.reason, "detail": e.detail, "field": e.field, "args": list(e.args)}
    return None


def canon(o):
    if type(o) is float and o.is_integer():
        return int(o)
    if isinstance(o, dict):
        return {k: canon(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [canon(v) for v in o]
    return o


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
        print(f"MISMATCH {G} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
