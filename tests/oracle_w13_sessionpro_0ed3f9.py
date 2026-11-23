#!/usr/bin/env python3
"""Differential oracle for gateway/session_prompt_pin.py (w13_sessionpro_0ed3f9).

sanitize_prompt_pin() is pure. get_prompt_pin() runs against a stubbed routing
index (_lock + _entry_locked are seams) so the stale-session guard is exercised
on the live module.
"""
import contextlib
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
QUARRY = "C:/Users/eman5/hermes-agent"
G = "w13_sessionpro_0ed3f9"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)

SRC = QUARRY + "/gateway/session_prompt_pin.py"
spec = importlib.util.spec_from_file_location(G + "_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[G + "_live"] = live
spec.loader.exec_module(live)

ENTRY = [None]
mixin = live.SessionPromptPinMixin()
mixin._lock = contextlib.nullcontext()
mixin._entry_locked = lambda key: ENTRY[0]


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
    if func == "sanitize":
        return live.sanitize_prompt_pin(r.get("pin"))
    if func == "get_pin":
        e = r.get("entry")
        ENTRY[0] = types.SimpleNamespace(**e) if isinstance(e, dict) else None
        return mixin.get_prompt_pin("key", expected_session_id=r.get("expected"))
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
