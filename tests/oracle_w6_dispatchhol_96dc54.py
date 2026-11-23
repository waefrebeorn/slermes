#!/usr/bin/env python3
"""Differential oracle for cron/dispatch_hold.py (gate w6_dispatchhol_96dc54)."""
import importlib.util
import json
import sys
import tempfile
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, QUARRY)
SRC = QUARRY + "/cron/dispatch_hold.py"
NAME = "w6_dhold_live"
spec = importlib.util.spec_from_file_location(NAME, SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live
spec.loader.exec_module(live)

tmp = tempfile.TemporaryDirectory()
dirp = Path(tmp.name)
held_file = dirp / "dispatch_held.json"

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
    content = r.get("content")
    if content is None:
        try:
            held_file.unlink()
        except OSError:
            pass
    else:
        held_file.write_text(content, encoding="utf-8")
    if func == "read":
        want = live.read(dirp)
    elif func == "grace_starts_at":
        want = live.grace_starts_at(dirp, r.get("due_at", 0.0), r.get("grace", 0.0))
    else:
        continue
    cases += 1
    if want != got:
        mism += 1
        print(f"MISMATCH {func} in={r} py={want!r} c={got!r}", file=sys.stderr)
tmp.cleanup()
print(f"ORACLE w6_dispatchhol_96dc54: {cases} cases, {mism} mismatches")
sys.exit(1 if mism or cases == 0 else 0)
