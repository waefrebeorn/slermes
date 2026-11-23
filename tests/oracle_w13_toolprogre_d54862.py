#!/usr/bin/env python3
"""Differential oracle for w13_toolprogre_d54862 (tui_gateway/tool_progress.py)."""
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
SRC = QUARRY + "/tui_gateway/tool_progress.py"
sys.path.insert(0, QUARRY)
if "tui_gateway" not in sys.modules:
    _pkg = types.ModuleType("tui_gateway")
    _pkg.__path__ = [QUARRY + "/tui_gateway"]
    sys.modules["tui_gateway"] = _pkg

spec = importlib.util.spec_from_file_location("tui_gateway.tool_progress", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["tui_gateway.tool_progress"] = live
spec.loader.exec_module(live)


def canon(v):
    def conv(x):
        if isinstance(x, float) and x.is_integer():
            return int(x)
        if isinstance(x, dict):
            return {k: conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False))


def compute(r):
    f = r.get("func")
    if f == "cap_tui_verbose_text":
        return live._cap_tui_verbose_text(r.get("text", ""))
    if f == "fmt_tool_duration":
        return live._fmt_tool_duration(r.get("seconds"))
    if f == "count_list":
        return live._count_list(r.get("obj"), *(r.get("path") or []))
    if f == "tool_summary":
        return live._tool_summary(r.get("name", ""), r.get("result", ""), r.get("duration_s"))
    if f == "normalize_todo_state":
        return live._normalize_todo_state(r.get("value"))
    if f == "tool_result_needs_user":
        return live._tool_result_needs_user(r.get("result", ""))
    if f == "int_or_skip":
        return live._int_or_skip(r.get("v"))
    if f == "str_list":
        return live._str_list(r.get("v") or [])
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
        py = canon(compute(r))
    except Exception as exc:
        print(f"ORACLE w13_toolprogre_d54862: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = canon(r.get("out"))
    if py != got:
        mism += 1
        print(f"MISMATCH w13_toolprogre_d54862 {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w13_toolprogre_d54862: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
