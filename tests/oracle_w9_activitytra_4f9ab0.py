#!/usr/bin/env python3
"""Differential oracle for agent/activity_tracking.py (gate w9_activitytra_4f9ab0).

AST-extracts ActivityTrackingMixin's three state-transition methods and replays
them with stubbed clock / session_activity / store seams driven by the echoed
fixture keys. Compares the resulting observable state to the harness "out".
"""
import ast
import contextlib
import json
import sys
import types
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/agent/activity_tracking.py"
KEEP = {"_touch_activity", "_persist_session_activity_if_due", "_reset_activity_labels_after_turn"}

ns = {}
tree = ast.parse(open(SRC, encoding="utf-8").read())
nodes = []
for n in tree.body:
    if isinstance(n, ast.ClassDef) and n.name == "ActivityTrackingMixin":
        for m in n.body:
            if isinstance(m, ast.FunctionDef) and m.name in KEEP:
                nodes.append(m)
exec(compile(ast.fix_missing_locations(ast.Module(body=nodes, type_ignores=[])), SRC, "exec"), ns)

_ns = sys.modules.get("agent")
if _ns is None:
    _ns = types.ModuleType("agent")
    _ns.__path__ = []
    sys.modules["agent"] = _ns

ns["suppress"] = contextlib.suppress
ns["logger"] = SimpleNamespace(debug=lambda *a, **k: None, info=lambda *a, **k: None)

class _NullLock:
    def __enter__(self): return self
    def __exit__(self, *a): return False
ns["_activity_lock"] = lambda self: _NullLock()

def _touch_marker(*a, **k): pass
def _clear_marker(*a, **k): pass

def setup(r):
    state = {"off": [], "reset": [], "persist": []}
    ns["time"] = SimpleNamespace(
        time=lambda: r.get("ts", 0.0),
        monotonic=lambda: r.get("now_mono", 0.0))
    ns["os"] = SimpleNamespace(environ={})

    sa = types.ModuleType("agent.session_activity")
    sa.ActivityProvenance = SimpleNamespace(UNKNOWN="unknown")
    sa.bound_activity_description = lambda d: r.get("bound_desc", d or "")
    sa.normalize_activity_provenance = lambda p: r.get("resolved_provenance", p)
    sa.is_terminal_compression_provenance = lambda p: bool(r.get("is_terminal", False))
    def reset_win(agent):
        state["reset"].append(1)
        agent._session_activity_last_persist_mono = 0.0
    sa.reset_session_activity_persist_window = reset_win
    sa.SESSION_ACTIVITY_HEARTBEAT_MIN_INTERVAL_SECONDS = r.get("interval", 60)
    sys.modules["agent.session_activity"] = sa
    ns["ActivityProvenance"] = sa.ActivityProvenance

    ais = types.ModuleType("agent.agent_init_session")
    def agent_interface(agent):
        if not r.get("has_session_db", False):
            return None
        return SimpleNamespace(sessions=SimpleNamespace(
            touch_activity=(_touch_marker if r.get("has_touch", False) else None),
            clear_activity_labels=(_clear_marker if r.get("has_clear", False) else None)))
    ais.agent_interface = agent_interface
    sys.modules["agent.agent_init_session"] = ais

    rss = types.ModuleType("agent.runtime_session_store")
    def call_off(fn, *a, **k):
        state["off"].append(fn)
    rss.call_off_event_loop = call_off
    sys.modules["agent.runtime_session_store"] = rss
    return state


def compute(r):
    func = r.get("func")
    state = setup(r)
    if func == "touch":
        self = SimpleNamespace()
        g = r.get("gen")
        if isinstance(g, (int, float)) and not isinstance(g, bool):
            self._turn_liveness_activity_generation = g
        self._persist_session_activity_if_due = lambda: state["persist"].append(1)
        ns["_touch_activity"](self, r.get("desc"),
                              provenance=r.get("provenance"),
                              force_persist=bool(r.get("force_persist", False)))
        return {
            "generation": self._turn_liveness_activity_generation,
            "last_activity_ts": self._last_activity_ts,
            "last_activity_desc": self._last_activity_desc,
            "last_activity_provenance": self._last_activity_provenance,
            "abort_claim": self._turn_liveness_abort_claim,
            "reset_window": bool(state["reset"]),
            "persist_called": bool(state["persist"]),
        }
    if func == "persist_if_due":
        self = SimpleNamespace(
            session_id=r.get("session_id"),
            _session_db=(object() if r.get("has_session_db", False) else None),
            _session_activity_last_persist_mono=r.get("last_mono", 0.0),
            _last_activity_ts=r.get("ts", 0.0),
            _last_activity_desc=r.get("bound_desc", ""),
            _last_activity_provenance=r.get("resolved_provenance", "unknown"))
        ns["_persist_session_activity_if_due"](self)
        return {
            "touched": any(fn is _touch_marker for fn in state["off"]),
            "new_last_mono": self._session_activity_last_persist_mono,
        }
    if func == "reset_labels":
        self = SimpleNamespace(
            session_id=r.get("session_id"),
            _session_db=(object() if r.get("has_session_db", False) else None),
            _last_activity_desc="stale",
            _last_activity_provenance="agent.compression")
        ns["_reset_activity_labels_after_turn"](self)
        return {
            "last_activity_desc": self._last_activity_desc,
            "last_activity_provenance": self._last_activity_provenance,
            "clear_called": any(fn is _clear_marker for fn in state["off"]),
        }
    return None


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
        py = compute(r)
    except Exception as exc:
        mism += 1
        print(f"MISMATCH case={r.get('case')} func={r.get('func')}: python raised {exc!r}", file=sys.stderr)
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH case={r.get('case')} func={r.get('func')} py={py!r} c={got!r}", file=sys.stderr)
if cases == 0:
    print("ORACLE w9_activitytra_4f9ab0: 0 cases (link failure in disguise)")
    sys.exit(1)
print(f"ORACLE w9_activitytra_4f9ab0: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
