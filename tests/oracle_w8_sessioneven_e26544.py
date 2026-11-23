#!/usr/bin/env python3
"""Differential oracle for upstream Hermes gateway/session_events.py
(w8_sessioneven_e26544 gate).

Heavy constructors are avoided: the unbound methods are driven with a fake
`self` carrying only `_progress` and a lock, and time.time() is injected per
case through the module namespace so frame timestamps are deterministic.
"""
import importlib.util
import json
import sys
import threading
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


tui = _mod("tui_gateway")
tui.__path__ = []
_mod("tui_gateway.event_replay",
     release_key=lambda *a, **k: None,
     latest_seq=lambda *a, **k: 0,
     events_since=lambda *a, **k: [],
     is_truncated=lambda *a, **k: False,
     _stamp_event=lambda frame: None)


class _Fanout:
    def __init__(self, *a, **k):
        pass

    def write(self, *a, **k):
        return False


_mod("tui_gateway.transport", FanoutTransport=_Fanout)

SRC = Q + "/gateway/session_events.py"
spec = importlib.util.spec_from_file_location("sessev_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["sessev_live"] = live
spec.loader.exec_module(live)


def fake_self(state):
    return types.SimpleNamespace(_progress=state, lock=threading.RLock())


def set_now(now):
    live.time = types.SimpleNamespace(time=lambda: now)


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func = r.get("func")
    try:
        if func == "note":
            me = fake_self(r.get("state"))
            set_now(r.get("now", 0))
            live.SessionEvents.note_progress(me, r.get("generation"),
                                              r.get("event_type", ""), r.get("payload"))
            py = me._progress
        elif func == "progress":
            me = fake_self(r.get("state"))
            py = live.SessionEvents.progress_for(me, r.get("generation"))
        elif func == "frames":
            me = fake_self(None)
            for f in r.get("frames", []):
                set_now(f.get("now", 0))
                live.SessionEvents.note_progress(me, f.get("generation"),
                                                  f.get("event_type", ""), f.get("payload"))
            py = live.SessionEvents.progress_for(me, r.get("generation"))
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w8_sessioneven_e26544: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
