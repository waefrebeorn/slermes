#!/usr/bin/env python3
"""Differential oracle for w13_debugredac_c651e1 (hermes_cli/debug_redaction.py)."""
import importlib.util
import json
import sys

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass

QUARRY = "C:/Users/eman5/hermes-agent"
SRC = QUARRY + "/hermes_cli/debug_redaction.py"
sys.path.insert(0, QUARRY)
import hermes_cli  # noqa: F401  parent-package bootstrap
spec = importlib.util.spec_from_file_location("hermes_cli.debug_redaction", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["hermes_cli.debug_redaction"] = live
spec.loader.exec_module(live)


def canon(v):
    def conv(x):
        if isinstance(x, float) and x.is_integer():
            return int(x)
        if isinstance(x, dict):
            return {k: conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False))


def compute(r):
    if r.get("func") == "decoded_compact_name":
        return live._decoded_compact_name(r.get("value"))
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
        py = canon(compute(r))
    except Exception as exc:
        print(f"ORACLE w13_debugredac_c651e1: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = canon(r.get("out"))
    if py != got:
        mism += 1
        print(f"MISMATCH w13_debugredac_c651e1 {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w13_debugredac_c651e1: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
