#!/usr/bin/env python3
"""Faithfulness oracle for port_agent_display.c shell-summarization helpers.

Reads JSON lines from t_port_display_shell.c and recomputes the SAME
functions from the LIVE agent/display.py.
"""
import sys, os, json
sys.path.insert(0, os.path.expanduser("~/hermes-agent-dev"))
import agent.display as D

mism = 0
n = 0
for line in sys.stdin:
    line = line.strip()
    if not line or not line.startswith("{"):
        continue
    rec = json.loads(line)
    fn = rec["fn"]
    n += 1
    try:
        if fn == "split_words":
            exp = D._split_shell_words(rec["in"])
        elif fn == "strip_pipe":
            exp = D._strip_shell_pipe_tail(rec["in"])
        elif fn == "clean_seg":
            exp = D._clean_shell_segment(rec["in"])
        elif fn == "compound":
            exp = D._split_shell_compound(rec["in"])
        elif fn == "boundary_echo":
            exp = 1 if D._is_shell_boundary_echo(rec["in"]) else 0
        else:
            print("UNKNOWN FN", fn); continue
    except Exception as e:
        print("ORACLE ERROR", fn, repr(rec), e)
        mism += 1
        continue
    got = rec["out"]
    ok = (exp == got)
    if not ok:
        mism += 1
        print(f"MISMATCH fn={fn} in={rec['in']!r} PY={exp!r} C={got!r}")
print(f"DISPLAY_SHELL oracle: {n} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
