#!/usr/bin/env python3
"""Differential oracle for gate w14_turnscript_3a854a (agent/turn_scripted_prelude.py)."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
QUARRY = "C:/Users/eman5/hermes-agent"
G = "w14_turnscript_3a854a"
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


# Deterministic call-id seam. The real helper hashes with sha256; the C port
# reproduces this injected stub byte-for-byte so the canonical-json formatting
# in prelude_call_id is still exercised differentially.
def _det(fn_name, arguments, index=0):
    return f"dcid:{fn_name}:{arguments}:{index}"


_seed("agent", __path__=[QUARRY + "/agent"])
_seed("agent.message_sanitization", deterministic_call_id=_det)
_seed("agent.transports", __path__=[QUARRY + "/agent/transports"])
_seed("agent.transports.types",
      NormalizedResponse=lambda **kw: NS(**kw),
      build_tool_call=lambda *a: NS())
_seed("agent.turn_tool_round", run_tool_round=lambda *a, **k: NS(action="continue"))

SRC = QUARRY + "/agent/turn_scripted_prelude.py"
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
    f = r.get("func")
    if f == "prelude_call_id":
        return live.prelude_call_id(r.get("session_id", ""), r.get("name", ""),
                                    r.get("args"), int(r.get("index", 0)))
    if f == "play_prelude":
        verdict_obj = r.get("verdict")
        v = None
        if verdict_obj is not None:
            v = NS(action=verdict_obj.get("action"), result=verdict_obj.get("result"))
        live.run_scripted_prelude = lambda a, s, p: v
        return list(live.play_prelude(None, None, object() if r.get("prelude_present") else None))
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
