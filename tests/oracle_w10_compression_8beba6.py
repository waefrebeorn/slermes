#!/usr/bin/env python3
"""Differential oracle for live agent/compression_marker.py (w10_compression_8beba6)."""
import importlib.util
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

SRC = Q + "/agent/compression_marker.py"
spec = importlib.util.spec_from_file_location("cm_8beba6_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["cm_8beba6_live"] = live
spec.loader.exec_module(live)


def compute(r):
    f = r.get("func")
    if f == "elision_marker":
        return live._elision_marker(int(r.get("omitted", "0")), int(r.get("total", "0")))
    if f == "elision_marker_max_len":
        return live.ELISION_MARKER_MAX_LEN
    if f == "elide":
        return live.elide(r.get("text", ""), int(r.get("limit", "0")))
    if f == "elide_middle":
        return live.elide_middle(r.get("text", ""), int(r.get("head", "0")), int(r.get("tail", "0")))
    return None


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
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        print(f"LIVE ERROR {r.get('func')}: {exc!r}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {r.get('func')} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w10_compression_8beba6: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
