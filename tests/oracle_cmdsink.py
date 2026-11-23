#!/usr/bin/env python3
"""Differential oracle for upstream Hermes gateway/session_command_sink.py."""
import importlib.util, json, sys
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/gateway/session_command_sink.py"
spec = importlib.util.spec_from_file_location("session_command_sink_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["session_command_sink_live"] = live
spec.loader.exec_module(live)

def compute(notices, prompt, display):
    outside_none = live.active_sink() is None
    with live.capture() as s:
        for n in notices:
            s.notices.append(n)
        s.prompt = prompt
        s.display = display
        inside = {"notices": s.notices, "prompt": s.prompt, "display": s.display}
        active_present = live.active_sink() is s
        inside_depth = 1
        with live.capture() as s2:
            nested_fresh = (s2.notices == [] and s2.prompt is None and s2.display is None)
            nested_depth = 2
        restored = live.active_sink() is s
    after_none = live.active_sink() is None
    return {
        "outside_none": outside_none,
        "inside": inside,
        "active_present": active_present,
        "inside_depth": inside_depth,
        "nested_fresh": nested_fresh,
        "nested_depth": nested_depth,
        "restored": restored,
        "after_none": after_none,
    }

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") != "scenario":
        continue
    cases += 1
    py = compute(r.get("notices", []), r.get("prompt"), r.get("display"))
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH scenario py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE cmdsink: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
