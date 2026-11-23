#!/usr/bin/env python3
"""Differential oracle for gate w10_turnmarker_94f189 (tui_gateway/turn_marker.py)."""
import importlib.util, json, os, sys, types
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)
_u = types.ModuleType("utils"); _u.atomic_json_write = lambda *a, **k: None
sys.modules["utils"] = _u
_hc = types.ModuleType("hermes_cli"); _hc.__path__ = [os.path.join(Q, "hermes_cli")]
sys.modules["hermes_cli"] = _hc
_as = types.ModuleType("hermes_cli.active_sessions")
_HOLDER = {"mode": 0}
def _pid_liveness(pid, started):
    mode = _HOLDER["mode"]
    if mode == 3:
        raise RuntimeError("liveness lookup failed")
    if mode == 0:
        return None
    return mode == 1
_as._pid_liveness = _pid_liveness
sys.modules["hermes_cli.active_sessions"] = _as

NAME = "w10_turnmarker_94f189_live"
sp = importlib.util.spec_from_file_location(NAME, os.path.join(Q, "tui_gateway", "turn_marker.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)

def compute(r):
    f = r.get("func")
    if f == "started_at":
        return live._started_at(r.get("entry"))
    if f == "marker_writer_state":
        _HOLDER["mode"] = r.get("liveness", 0)
        return live.marker_writer_state(r.get("entry"))
    if f == "prune":
        return live._prune(r.get("entries") or {}, r.get("now", 0))
    if f == "read_turn_marker":
        live._load = lambda p: r.get("stored") or {}
        return live.read_turn_marker("/home", r.get("session_key", ""))
    return None

n = m = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if not isinstance(r, dict) or "out" not in r:
        continue
    n += 1
    py, got = compute(r), r.get("out")
    if py != got:
        m += 1
        print("MISMATCH %s py=%r c=%r" % (r.get("func"), py, got), file=sys.stderr)
print("ORACLE w10_turnmarker_94f189: %d cases, %d mismatches" % (n, m))
sys.exit(1 if m else 0)
