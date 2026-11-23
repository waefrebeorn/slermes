#!/usr/bin/env python3
"""Differential oracle for gate w14_sessionbot_859d7d (gateway/session_bot_outcome.py)."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
QUARRY = "C:/Users/eman5/hermes-agent"
G = "w14_sessionbot_859d7d"
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


_seed("gateway")
_seed("gateway.run",
      _INTERRUPT_REASON_GATEWAY_RESTART="Gateway restarting",
      _INTERRUPT_REASON_GATEWAY_SHUTDOWN="Gateway shutting down")
_MARKS = {"[SILENT]", "SILENT", "NO_REPLY", "NO REPLY", "[静默]", "静默", "[沉默]", "沉默"}
_seed("gateway.response_filters",
      is_intentional_silence_response=lambda s: isinstance(s, str) and s.strip() in _MARKS)

SRC = QUARRY + "/gateway/session_bot_outcome.py"
_spec = importlib.util.spec_from_file_location(G + "_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules[G + "_live"] = live
_spec.loader.exec_module(live)


def compute(r):
    func = r.get("func")
    if func == "owner_stopped":
        return live.owner_stopped(r.get("result") or {})
    if func == "reply_attempt":
        return live._reply_attempt(r.get("call"), r.get("sender", ""))
    if func == "last_words":
        return live._last_words(r.get("messages") or [], r.get("sender", ""))
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
