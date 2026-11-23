#!/usr/bin/env python3
"""Differential oracle for gate w12_sessionanc_9a64bb (gateway/session_ancillary.py)."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
QUARRY = "C:/Users/eman5/hermes-agent"
G = "w12_sessionanc_9a64bb"
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


for _pkg in ("gateway", "persistence", "persistence.core"):
    if _pkg not in sys.modules:
        _seed(_pkg)
_seed("gateway.session_busy_controls", authorize=lambda *a, **k: None)
_seed("persistence.core.errors", AuthorityRefusal=type("AuthorityRefusal", (RuntimeError,), {}))

SRC = QUARRY + "/gateway/session_ancillary.py"
_spec = importlib.util.spec_from_file_location(G + "_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules[G + "_live"] = live
_spec.loader.exec_module(live)

NS = types.SimpleNamespace


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
    if func == "subagent_row":
        rec = dict(r.get("record") or {})
        agent = rec.get("agent")
        if isinstance(agent, dict):
            rec["agent"] = NS(**agent)
        return live.subagent_row(rec)
    if func == "reachable_upload_home":
        sibs = [NS(profile_id=s) for s in (r.get("siblings") or [])]
        conn = NS(
            authority=NS(profile_id=r.get("authority_home"),
                         runner=NS(session_authorities=sibs)),
            host_scoped=bool(r.get("host_scoped")),
        )
        return live._reachable_upload_home(conn, r.get("raw_home"))
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
