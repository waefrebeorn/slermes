#!/usr/bin/env python3
"""
sta_oracle_web_base64_img.py — oracle for web_convert_base64_images_to_links.
Recomputes tools/web_tools.py:convert_base64_images_to_links on the SAME
inputs the C harness emitted, and exact-compares. When C disagrees, the bug
is in C (fix the C, never the oracle).
"""
import json, subprocess, sys, re

# import the LIVE parent-repo Python module (NOT inside slermes/)
sys.path.insert(0, "/home/wubu/hermes-agent-dev")
from tools.web_tools import convert_base64_images_to_links as py_fn

# the C harness prints one JSON line per case
proc = subprocess.run(
    ["/tmp/t_port_web_base64_img"],
    capture_output=True, text=True,
)
if proc.returncode != 0:
    print("HARNESS CRASHED:", proc.stderr); sys.exit(2)

lines = [l for l in proc.stdout.splitlines() if l.strip()]
mism = 0; tot = 0
for ln in lines:
    rec = json.loads(ln)
    tot += 1
    inp = rec["in"]
    c_out = rec["out"]
    py_out = py_fn(inp)
    if c_out != py_out:
        mism += 1
        if mism <= 12:
            print(f"MISMATCH in={inp!r}")
            print(f"  C : {c_out!r}")
            print(f"  PY: {py_out!r}")
print(f"RESULT: {tot - mism}/{tot} match, {mism} mismatch")
sys.exit(1 if mism else 0)
