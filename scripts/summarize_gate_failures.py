#!/usr/bin/env python3
"""Summarize first-attempt gate failure logs left by _oracle_one.sh."""
import glob
import os
import sys

files = sorted(glob.glob(".testbin/oracle_fail_*.log"))
if not files:
    print("no first-attempt failure logs")
    sys.exit(0)
for p in files:
    name = os.path.basename(p)[13:-4]
    lines = open(p, encoding="utf-8", errors="replace").read().splitlines()
    interesting = [l for l in lines if l.startswith("MISMATCH")
                   or "ERROR" in l or "Traceback" in l
                   or "error:" in l or "rc 127" in l or "no ORACLE" in l]
    print(f"=== {name} ({len(lines)} lines)")
    for l in (interesting or lines)[:3]:
        print("  " + l[:300])
