#!/usr/bin/env python3
"""Differential oracle for tools/approval_quoted_newlines.py (gate appquote)."""
import importlib.util
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

# _mask_quoted_newlines imports _scan_shell from tools.approval_parser_budget.
sys.path.insert(0, "C:/Users/eman5/hermes-agent")
SRC = "C:/Users/eman5/hermes-agent/tools/approval_quoted_newlines.py"
spec = importlib.util.spec_from_file_location("appquote_live", SRC)
live = importlib.util.module_from_spec(spec)
spec.loader.exec_module(live)

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
    got = r.get("out")
    cmd = r.get("command", "")
    if func == "mask":
        want = live._mask_quoted_newlines(cmd)
    elif func == "mask_span":
        s = int(r.get("start", "0"))
        es = r.get("end", "")
        e = int(es) if es else len(cmd)
        want = live._mask_quoted_newlines_span(cmd, s, e)
    else:
        continue
    cases += 1
    if want != got:
        mism += 1
        print(f"MISMATCH in={r} py={want!r} c={got!r}", file=sys.stderr)
print(f"ORACLE appquote: {cases} cases, {mism} mismatches")
sys.exit(1 if mism or cases == 0 else 0)
