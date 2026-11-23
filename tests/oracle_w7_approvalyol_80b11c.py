#!/usr/bin/env python3
"""Differential oracle for tools/approval_yolo.py (gate w7_approvalyol_80b11c).

The LIVE module is loaded by path with tools.approval stubbed: that module is
just the in-memory per-key bypass set plus a frozen flag and a launch-applied
set, all of which are injected per case from the fixture. The decision logic
under test is approval_yolo's own.
"""
import contextlib, importlib.util, json, sys, types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

tools_pkg = types.ModuleType("tools")
tools_pkg.__path__ = [Q + "/tools"]
sys.modules["tools"] = tools_pkg

STATE = {"enabled": {}, "applied": set(), "yo": set()}
appr = types.ModuleType("tools.approval")


def _is_enabled(k):
    return bool(STATE["enabled"].get(k, False))


def _enable(k):
    STATE["enabled"][k] = True
    STATE["yo"].add(k)


def _disable(k):
    STATE["enabled"].pop(k, None)
    STATE["yo"].discard(k)


appr.is_session_yolo_enabled = _is_enabled
appr.enable_session_yolo = _enable
appr.disable_session_yolo = _disable
appr._YOLO_MODE_FROZEN = False
appr._lock = contextlib.nullcontext()
appr._launch_yolo_applied = STATE["applied"]
appr._session_yolo = STATE["yo"]
sys.modules["tools.approval"] = appr

costs = types.ModuleType("tools.mcp_tool_costs")
costs.transfer_session = lambda *a, **k: None
sys.modules["tools.mcp_tool_costs"] = costs

SRC = Q + "/tools/approval_yolo.py"
spec = importlib.util.spec_from_file_location("w7_approvalyol_80b11c_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w7_approvalyol_80b11c_live"] = live
spec.loader.exec_module(live)


def reset(r):
    STATE["enabled"].clear()
    STATE["applied"].clear()
    STATE["yo"].clear()
    appr._YOLO_MODE_FROZEN = bool(r.get("frozen", 0))


def compute(r):
    func = r.get("func")
    reset(r)
    if func == "with_session_yolo":
        if r.get("enabled", 0):
            STATE["enabled"]["k"] = True
        return live.with_session_yolo(r.get("model_config"), "k")
    if func == "toggle":
        STATE["enabled"]["k"] = bool(r.get("current", 0))
        enabled = r["enabled"] if r.get("has_enabled", 0) else None
        return bool(live.toggle_session_yolo("k", enabled, persisted=bool(r.get("persisted", 0))))
    if func == "restore":
        key = "" if r.get("key_empty", 0) else "k"
        if r.get("current", 0):
            STATE["enabled"]["k"] = True
        return bool(live.restore_session_yolo(key, bool(r.get("persisted", 0))))
    if func == "apply_launch":
        if r.get("applied", 0):
            STATE["applied"].add("k")
        live.apply_launch_yolo("" if r.get("key_empty", 0) else "k")
        return _is_enabled("k")
    if func == "transfer":
        old = "" if r.get("old_empty", 0) else "old"
        if r.get("same", 0):
            new = old
        else:
            new = "" if r.get("new_empty", 0) else "new"
        if r.get("old_enabled", 0):
            STATE["enabled"]["old"] = True
        live.transfer_session_yolo(old, new)
        return _is_enabled("new")
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
    py = compute(r)
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {r.get('func')} args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w7_approvalyol_80b11c: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
