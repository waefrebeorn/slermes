#!/usr/bin/env python3
"""Oracle for gate w17_delegateto_5314c5 (tools/delegate_tool_child_run.py)."""
import importlib.util
import json
import sys
import types
import contextlib
import io
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, QUARRY)

def _load(dotted, rel):
    parts = dotted.split(".")
    for i in range(1, len(parts)):
        pn = ".".join(parts[:i])
        if pn not in sys.modules:
            pkg = types.ModuleType(pn)
            pkg.__path__ = [QUARRY + "/" + "/".join(parts[:i])]
            sys.modules[pn] = pkg
    spec = importlib.util.spec_from_file_location(dotted, QUARRY + "/" + rel)
    m = importlib.util.module_from_spec(spec)
    sys.modules[dotted] = m
    with contextlib.redirect_stderr(io.StringIO()):
        spec.loader.exec_module(m)
    return m

live = _load("tools.delegate_tool_child_run", "tools/delegate_tool_child_run.py")

def _child(r):
    return SimpleNamespace(**(r.get("child") or {}))

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
    if func == "num":
        py = live._num(r.get("value"), r.get("default", 0))
    elif func == "str_or_none":
        py = live._str_or_none(r.get("value"))
    elif func == "is_image_url":
        py = live._is_image_url(r.get("ref", ""))
    elif func == "budget_warning_text":
        py = live._budget_warning_text(r.get("idle", 0), r.get("timeout", 0))
    elif func == "fabricated_entry":
        py = live._fabricated_entry(int(r.get("idx", 0)), r.get("status", ""), r.get("error"),
                                    _child(r), r.get("duration", 0))
    elif func == "append_missed_steer":
        e = dict(r.get("entry") or {})
        live._append_missed_steer(e, r.get("late"))
        py = e
    elif func == "tier_in_effect":
        py = live._tier_in_effect(_child(r))
    elif func == "constants":
        py = {
            "_HEARTBEAT_INTERVAL": live._HEARTBEAT_INTERVAL,
            "_HEARTBEAT_STALE_CYCLES_IDLE": live._HEARTBEAT_STALE_CYCLES_IDLE,
            "_HEARTBEAT_STALE_CYCLES_IN_TOOL": live._HEARTBEAT_STALE_CYCLES_IN_TOOL,
            "_STALE_RESULT_GRACE_SECONDS": live._STALE_RESULT_GRACE_SECONDS,
            "_LIVENESS_POLL_SECONDS": live._LIVENESS_POLL_SECONDS,
            "_BUDGET_WARNING_FRACTION": live._BUDGET_WARNING_FRACTION,
        }
    else:
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w17_delegateto_5314c5: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
