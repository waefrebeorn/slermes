#!/usr/bin/env python3
"""Faithfulness oracle for port_tools_tool_result_storage.c:_safe_result_filename.

Reads JSON lines emitted by t_port_tool_result_storage_filename.c and
recomputes the SAME function from the LIVE tools/tool_result_storage.py.
"""
import sys, os, json
sys.path.insert(0, os.path.expanduser("~/hermes-agent-dev"))
from tools.tool_result_storage import _safe_result_filename

mism = 0
n = 0
for line in sys.stdin:
    line = line.strip()
    if not line or not line.startswith("{"):
        continue
    rec = json.loads(line)
    fn = rec["fn"]
    if fn != "safe_result_filename":
        print("UNKNOWN FN", fn); continue
    n += 1
    tid = rec["id"]
    exp = _safe_result_filename(tid if tid != "" else None)
    got = rec["out"]
    if exp != got:
        mism += 1
        print(f"MISMATCH id={tid!r} PY={exp!r} C={got!r}")
print(f"SAFE_RESULT_FILENAME oracle: {n} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
