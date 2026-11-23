#!/usr/bin/env python3
"""Differential oracle for upstream Hermes pm/extras.py (gate w5_extras_3292f9).

The pure name/anchor helpers are compared directly. Availability probing is
side-effectful, so `missing` gets an injected importable set through the
module's own _importable seam.
"""
import importlib.util
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

SRC = Q + "/pm/extras.py"
spec = importlib.util.spec_from_file_location("w5_extras_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w5_extras_live"] = live
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
    extra = r.get("extra", "")
    try:
        if func == "canonical":
            py = live._canonical(r.get("name", ""))
        elif func == "hint":
            py = live.install_hint(extra)
        elif func == "anchors":
            py = list(live._anchors(extra))
        elif func == "missing":
            available = set(r.get("importable") or [])
            live._importable = lambda a, s=available: a in s
            py = list(live.missing(extra))
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w5_extras_3292f9: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
