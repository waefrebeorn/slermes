#!/usr/bin/env python3
"""Differential oracle for w7_platformenv_955637 (gateway/platform_env.py)."""
import importlib.util
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

SRC = Q + "/gateway/platform_env.py"
spec = importlib.util.spec_from_file_location("w7_platformenv_955637_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w7_platformenv_955637_live"] = live
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
        if func == "csv_list":
            py = live.csv_list(r.get("value", ""))
        elif func == "strip_slash":
            py = live.strip_slash(r.get("value", ""))
        elif func == "strip_lower":
            py = live.strip_lower(r.get("value", ""))
        elif func == "truthy":
            py = live.truthy_token(r.get("value", ""))
        elif func == "int_or":
            py = live.int_or(int(r.get("default", 0)))(r.get("value", ""))
        elif func == "mention":
            py = live.mention_patterns(r.get("value", ""))
        elif func == "env_first":
            values = r.get("values") or {}
            live.getenv = lambda name: values.get(name, "")
            py = live.env_first(r.get("names"))
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} {r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} {r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w7_platformenv_955637: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
