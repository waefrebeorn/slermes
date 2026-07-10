#!/usr/bin/env python3
"""Oracle for v552 fixes: file_ops_looks_like_linter_unusable + delete_path guard.

Compares C against LIVE tools/file_operations.py._looks_like_linter_unusable
and Python's is_write_denied (via agent.file_safety) for delete_path denials.

  gcc -O2 -I include -I src/tools -I lib/libjson \
      tests/t_port_file_ops_lint.c src/tools/port_file_operations.o \
      src/agent/file_safety.o lib/libjson/json.o -o /tmp/t_lint
  /tmp/t_lint | python3 tests/sta_oracle_file_ops_lint.py
"""
import json, sys, os
REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, REPO)
import tools.file_operations as fo
import agent.file_safety as fs

# Both C is_write_denied and Python agent.file_safety.is_write_denied resolve
# '~' against the real HERMES_HOME; we compare the tilde form directly.

mismatches = 0
total = 0
for line in sys.stdin:
    line = line.strip()
    if not line: continue
    rec = json.loads(line)
    fn = rec["fn"]
    total += 1
    if fn == "looks_like_linter_unusable":
        bc = rec["base_cmd"]
        out = rec["out"]
        exp = fo._looks_like_linter_unusable(bc, out)
        got = (rec["res"] is True)
        if got != exp:
            mismatches += 1
            print(f"MISMATCH {fn} base_cmd={bc!r} out={out!r} C={got} PY={exp}")
    elif fn == "delete_path_denied":
        p = rec["base_cmd"]
        # Python's _is_write_denied(path) -> C is_write_denied(path) (same port)
        exp = fs.is_write_denied(p)  # True -> denied -> C returns false
        got = (rec["res"] is True)
        if got != (not exp):
            mismatches += 1
            print(f"MISMATCH {fn} path={p!r} C={got} (denied={exp})")
    elif fn == "delete_path_ok":
        # a real temp file should delete successfully
        if rec["res"] is not True:
            mismatches += 1
            print(f"MISMATCH {fn} path={rec['base_cmd']!r} C={rec['res']} (expected true)")

print(f"{total} cases, {mismatches} mismatches")
sys.exit(1 if mismatches else 0)
