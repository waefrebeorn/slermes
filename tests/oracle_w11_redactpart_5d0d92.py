#!/usr/bin/env python3
"""Differential oracle for agent/redact_partial_keys.py (gate w11_redactpart_5d0d92).

The live module's only top-level import is stdlib re, so it loads unmodified.
Each case supplies a "text" string; the live line-walk is compared byte-for-byte
with the C port's output.
"""
import importlib.util
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)

SRC = QUARRY + "/agent/redact_partial_keys.py"
spec = importlib.util.spec_from_file_location("w11_redactpart_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w11_redactpart_live"] = live
spec.loader.exec_module(live)


def compute(r):
    if r.get("func") == "redact":
        return live._redact_partial_private_keys(r.get("text", ""))
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
        mism += 1
        print(f"MISMATCH case={r.get('case')} func={r.get('func')}: python raised {exc!r}", file=sys.stderr)
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH case={r.get('case')} func={r.get('func')} py={py!r} c={got!r}", file=sys.stderr)
if cases == 0:
    print("ORACLE w11_redactpart_5d0d92: 0 cases (link failure in disguise)")
    sys.exit(1)
print(f"ORACLE w11_redactpart_5d0d92: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
