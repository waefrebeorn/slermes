#!/usr/bin/env python3
"""Oracle for gate w18_filetools_b76f5d (tools/file_tools_write_guards.py)."""
import importlib.util
import json
import sys
import types
import contextlib
import io

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


live = _load("tools.file_tools_write_guards", "tools/file_tools_write_guards.py")

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
        if func == "is_internal_file_status_text":
            py = live._is_internal_file_status_text(r.get("content", ""))
        elif func == "looks_like_read_file_line_numbered_content":
            py = live._looks_like_read_file_line_numbered_content(r.get("content", ""))
        elif func == "is_internal_file_tool_content":
            py = live._is_internal_file_tool_content(r.get("content", ""))
        elif func == "stale_write_refusal":
            py = live._stale_write_refusal(r.get("filepath", ""), r.get("reason", ""), r.get("resolved"))
        else:
            continue
    except Exception as exc:
        print(f"SKIP {func}: {exc}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} in={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w18_filetools_b76f5d: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
