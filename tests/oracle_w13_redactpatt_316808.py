#!/usr/bin/env python3
"""Differential oracle for agent/redact_pattern_shape.py (w13_redactpatt_316808)."""
import importlib.util
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

SRC = Q + "/agent/redact_pattern_shape.py"
spec = importlib.util.spec_from_file_location("redact_pattern_shape_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["redact_pattern_shape_live"] = live
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
    try:
        if func == "skip_class":
            py = live._skip_char_class(r.get("pattern", ""), int(r.get("i", 0)))
        elif func == "unbounded":
            py = live._unbounded_quantifier_follows(r.get("pattern", ""), int(r.get("j", 0)))
        elif func == "structure":
            py = list(live._pattern_structure(r.get("pattern", "")))
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} case={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} case={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w13_redactpatt_316808: {cases} cases, {mism} mismatches")
sys.exit(1 if mism or cases == 0 else 0)
