#!/usr/bin/env python3
"""Differential oracle for hermes_cli/fs_utils.py (gate w7_fsutils_903462).

Both the harness and this oracle build the same directory from the echoed
fixture keys, apply the same read-only bits, and run their respective
rmtree_force. The compared result is whether the path was fully removed.
"""
import importlib.util
import json
import os
import shutil
import stat
import sys
from pathlib import Path

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

spec = importlib.util.spec_from_file_location(
    "w7_fsutils_live", QUARRY + "/hermes_cli/fs_utils.py")
live = importlib.util.module_from_spec(spec)
sys.modules["w7_fsutils_live"] = live
spec.loader.exec_module(live)


def compute(r):
    path = Path(r.get("path") or "")
    shutil.rmtree(path, ignore_errors=True)
    files = r.get("files") or []
    readonly = set(r.get("readonly") or [])
    if not files:
        path.mkdir(parents=True, exist_ok=True)
    for rel in files:
        target = path / rel
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(b"x")
        if rel in readonly:
            os.chmod(target, stat.S_IREAD)
    live.rmtree_force(path)
    return not path.exists()


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") is None:
        continue
    try:
        py = compute(r)
    except Exception as exc:
        print("ORACLE w7_fsutils_903462: live raised %r case %r" % (exc, r.get("case")),
              file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    cases += 1
    if py != got:
        mism += 1
        print("MISMATCH w7_fsutils_903462 case=%s py=%r c=%r" %
              (r.get("case"), py, got), file=sys.stderr)

print("ORACLE w7_fsutils_903462: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
