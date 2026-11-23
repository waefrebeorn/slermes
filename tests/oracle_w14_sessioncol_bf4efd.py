#!/usr/bin/env python3
"""Differential oracle for gate w14_sessioncol_bf4efd (gateway/session_cold_review.py)."""
import asyncio
import copy
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
QUARRY = "C:/Users/eman5/hermes-agent"
G = "w14_sessioncol_bf4efd"
sys.path.insert(0, QUARRY)
NS = types.SimpleNamespace

SRC = QUARRY + "/gateway/session_cold_review.py"
_spec = importlib.util.spec_from_file_location(G + "_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules[G + "_live"] = live
_spec.loader.exec_module(live)


def _run(r):
    async def peek(key):
        return r.get("session_id")
    async def load(sid):
        return copy.deepcopy(r.get("history") or [])
    fake = NS(async_session_store=NS(peek_session_id=peek, load_transcript=load))
    return asyncio.run(live.stored_transcript(fake, r.get("session_key", "k")))


def compute(r):
    func = r.get("func")
    sid, hist = _run(r)
    if func == "stored_transcript":
        return {"session_id": sid, "history": hist}
    if func == "is_conversational":
        return bool(hist)
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
