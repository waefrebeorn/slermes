#!/usr/bin/env python3
"""Differential oracle for gateway/session_retirement.py (w13_sessionret_ac44c5).

The authority registry, locks and the running loop are seams: _pinned and
_reviewing are evaluated on the live module against fixture-built live-session
objects. Platform identity is projected as the Platform enum's string value.
"""
import asyncio
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
QUARRY = "C:/Users/eman5/hermes-agent"
G = "w13_sessionret_ac44c5"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)


def stub(name, **attrs):
    m = types.ModuleType(name)
    m.__path__ = []
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


if "gateway" not in sys.modules:
    stub("gateway")
stub("gateway.config", Platform=types.SimpleNamespace(LOCAL="local", API_SERVER="api_server"))
HISTORY = [False]
stub("gateway.session_history_mount", is_history_mount=lambda live: HISTORY[0])

SRC = QUARRY + "/gateway/session_retirement.py"
spec = importlib.util.spec_from_file_location(G + "_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[G + "_live"] = live
spec.loader.exec_module(live)


def build_live(r):
    task_kind = r.get("task", "none")
    if task_kind == "none":
        task = None
    elif task_kind == "current":
        task = asyncio.current_task()
    else:
        task = types.SimpleNamespace(done=lambda k=task_kind: k == "done")

    review = types.SimpleNamespace()
    if r.get("review_run"):
        review._background_review_run = types.SimpleNamespace(
            request_done=types.SimpleNamespace(is_set=lambda: bool(r.get("request_done"))))

    source = None
    if r.get("source") is not None:
        source = types.SimpleNamespace(platform=r["source"].get("platform"))

    return types.SimpleNamespace(
        source=source,
        subscribers=bool(r.get("subscribers")),
        event_stream=types.SimpleNamespace(observers=bool(r.get("observers"))),
        rescan=bool(r.get("rescan")), settling=bool(r.get("settling")),
        detached=bool(r.get("detached")), detaching=bool(r.get("detaching")),
        mutation_lock=types.SimpleNamespace(locked=lambda: bool(r.get("mutation_locked"))),
        claim_gate=types.SimpleNamespace(locked=lambda: bool(r.get("claim_locked"))),
        task=task, review=review,
    )


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

    async def go():
        HISTORY[0] = bool(r.get("history_mount"))
        obj = build_live(r)
        if func == "reviewing":
            return live._reviewing(obj)
        sid = r.get("session_id", "s")
        authority = types.SimpleNamespace(
            sessions={sid: obj} if r.get("mapped") else {},
            executing={sid} if r.get("in_executing") else set(),
            pending_closes={sid} if r.get("in_pending_closes") else set(),
            orphan_closes={sid} if r.get("in_orphan_closes") else set(),
        )
        return live._pinned(authority, sid, obj)

    return asyncio.run(go())


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
