#!/usr/bin/env python3
"""Differential oracle for w13_plugininje_70086e (tui_gateway/plugin_inject.py)."""
import importlib.util
import json
import sys
import types

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass

QUARRY = "C:/Users/eman5/hermes-agent"
SRC = QUARRY + "/tui_gateway/plugin_inject.py"
sys.path.insert(0, QUARRY)
if "tui_gateway" not in sys.modules:
    _pkg = types.ModuleType("tui_gateway")
    _pkg.__path__ = [QUARRY + "/tui_gateway"]
    sys.modules["tui_gateway"] = _pkg

spec = importlib.util.spec_from_file_location("tui_gateway.plugin_inject", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["tui_gateway.plugin_inject"] = live
spec.loader.exec_module(live)


class NoopLock:
    def __enter__(self): return self
    def __exit__(self, *a): return False


class FakeThread:
    def __init__(self, *a, **k): pass
    def start(self): pass


def compute(r):
    sessions = {}
    for i, s in enumerate(r.get("sessions") or []):
        s = dict(s)
        s["history_lock"] = NoopLock() if s.get("history_lock") is not None else None
        sessions[str(i)] = s
    live._sessions = sessions
    live._sessions_lock = NoopLock()
    live._enqueue_prompt = lambda session, content, keep_transport: None
    live._drain_queued_prompt = lambda *a: None
    live.time = types.SimpleNamespace(time=lambda: 0.0)
    live.threading = types.SimpleNamespace(Thread=FakeThread)
    live.uuid = types.SimpleNamespace(uuid4=lambda: types.SimpleNamespace(hex="abcdef1234567890"))
    return live.inject_tui_session_message(session_key=r.get("session_key"),
                                           content=r.get("content"), plugin_id="")


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    try:
        py = bool(compute(r))
    except Exception as exc:
        print(f"ORACLE w13_plugininje_70086e: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH w13_plugininje_70086e case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w13_plugininje_70086e: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
