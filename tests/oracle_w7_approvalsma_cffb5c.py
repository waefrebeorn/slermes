#!/usr/bin/env python3
"""Differential oracle for tools/approval_smart.py (gate w7_approvalsma_cffb5c).

Compares the pure shell-comment/verdict helpers against the LIVE module.
"""
import importlib.util
import json
import sys

for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

import tools  # noqa: F401  (parent-package bootstrap)
spec = importlib.util.spec_from_file_location("tools.approval_smart", Q + "/tools/approval_smart.py")
live = importlib.util.module_from_spec(spec)
sys.modules["tools.approval_smart"] = live
spec.loader.exec_module(live)


def verdict(answer):
    a = (answer or "").strip().upper()
    return live._VERDICTS.get(a, "escalate")


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    func = r.get("func")
    try:
        if func == "next_comment":
            py = live._next_comment(r.get("text", ""), int(r.get("start", 0)))
        elif func == "comment_spans":
            py = [[a, b] for a, b in live._comment_spans(r.get("text", ""))]
        elif func == "strip_line_comment":
            py = live._strip_line_comment(r.get("text", ""))
        elif func == "strip_shell_comments":
            py = live._strip_shell_comments(r.get("command", ""))
        elif func == "has_heredoc":
            py = live._has_heredoc(r.get("command", ""), live._comment_spans(r.get("command", "")))
        elif func == "verdict":
            py = verdict(r.get("answer"))
        else:
            continue
    except Exception as exc:
        print("LIVE ERROR %s %r: %r" % (func, r, exc), file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (func, r.get("case"), py, got), file=sys.stderr)

print("ORACLE w7_approvalsma_cffb5c: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
