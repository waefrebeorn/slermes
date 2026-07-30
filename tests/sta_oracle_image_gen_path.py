#!/usr/bin/env python3
"""
sta_oracle_image_gen_path.py — oracle for image_gen_path.
Recomputes the SAME laf cases against LIVE
tools/image_generation_tool.py:_looks_like_absolute_file_path and
exact-compares. When C disagrees, fix the C.
"""
import json, subprocess, sys
sys.path.insert(0, "/home/wubu/hermes-agent-dev")
import tools.image_generation_tool as igt

proc = subprocess.run(["/tmp/t_port_image_gen_path"], capture_output=True, text=True)
if proc.returncode != 0:
    print("HARNESS CRASHED:", proc.stderr); sys.exit(2)

total = 0; mism = 0
for ln in proc.stdout.splitlines():
    if not ln.strip(): continue
    rec = json.loads(ln)
    total += 1
    fn = rec["fn"]; inp = rec["in"]; cout = rec["out"]
    assert fn == "laf"
    exp = igt._looks_like_absolute_file_path(inp)
    cval = bool(cout)  # harness emits JSON boolean
    if exp != cval:
        mism += 1
        print(f"MISMATCH laf in={inp!r}\n  C : {cval}\n  PY: {exp}")

print(f"RESULT: {total - mism}/{total} match, {mism} mismatch")
sys.exit(1 if mism else 0)
