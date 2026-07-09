#!/usr/bin/env python3
"""Oracle: prove C cron_prompt_sanitize == LIVE tools/cronjob_tools.py.

Reads JSON lines {fn,in,out} from stdin (emitted by t_port_cron_prompt_sanitize),
recomputes the SAME function from the live Python module, and asserts equality.
Calls the REAL Python functions (no re-implementation). Exits 1 on mismatch.

  gcc -O2 -I include -I src/tools t_port_cron_prompt_sanitize.c \
      src/tools/cron_prompt_sanitize.o lib/libjson/json.o -o /tmp/t_cps
  /tmp/t_cps | python3 sta_oracle_cron_prompt_sanitize.py
"""
import json
import sys
import os

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, REPO)
import tools.cronjob_tools as py


def py_check_invisible(prompt: str) -> str:
    return py._check_invisible_unicode(prompt)


def py_strip_invisible(prompt: str):
    # Live fn returns (cleaned, removed_list). Normalize to C's shape:
    # C emits {"cleaned":str, "removed":["U+XXXX",...]} (removed sorted).
    cleaned, removed = py._strip_invisible_unicode(prompt)
    return json.dumps({"cleaned": cleaned, "removed": sorted(removed)}, sort_keys=True)


def py_scan_skill(prompt: str):
    cleaned, error = py._scan_cron_skill_assembled(prompt)
    return json.dumps({"cleaned": cleaned, "error": error}, sort_keys=True)


EXPECT = {
    "check_invisible_unicode": lambda i: py_check_invisible(i),
    "strip_invisible_unicode": py_strip_invisible,
    "scan_cron_skill_assembled": py_scan_skill,
}

total = 0
mismatches = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    rec = json.loads(line)
    fn, in_s, out_s = rec["fn"], rec["in"], rec["out"]
    try:
        exp = EXPECT[fn](in_s)
    except Exception as e:  # noqa: BLE001
        print(f"PYTHON ERROR {fn} in={in_s!r}: {e}")
        mismatches += 1
        total += 1
        continue
    total += 1
    # Structural comparison: both C `out` and Python `exp` are JSON strings.
    try:
        c_json = json.loads(out_s) if out_s else None
        p_json = json.loads(exp) if exp else None
    except Exception:
        c_json, p_json = out_s, exp
    if c_json != p_json:
        mismatches += 1
        print(f"MISMATCH {fn} in={in_s!r}\n  C  ={out_s!r}\n  PY ={exp!r}")

print(f"{total} cases, {mismatches} mismatches")
sys.exit(1 if mismatches else 0)
