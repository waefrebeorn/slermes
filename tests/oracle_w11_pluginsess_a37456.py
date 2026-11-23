#!/usr/bin/env python3
"""Differential oracle for agent/plugin_session_fork.py (gate w11_pluginsess_a37456).

The module's top-level imports are all stdlib, so the live module loads
unmodified.  Decimal-string ints are converted to Python int before calling
_bounded_int, matching the C port's strtoll seam.
"""
import importlib.util
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)

SRC = QUARRY + "/agent/plugin_session_fork.py"
spec = importlib.util.spec_from_file_location("w11_pluginsess_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w11_pluginsess_live"] = live
spec.loader.exec_module(live)


def _int(v):
    """Decimal-string -> int; non-int values pass through unchanged."""
    if isinstance(v, str):
        try:
            return int(v, 10)
        except ValueError:
            return v
    return v


def compute(r):
    f = r.get("func")
    if f == "bounded_int":
        hi = r.get("high", None)
        return live._bounded_int(
            _int(r.get("value")), _int(r.get("low")),
            None if hi is None else _int(hi))
    if f == "clamped_timeout":
        return live._clamped_timeout(r.get("timeout"))
    if f == "constants":
        return {k: getattr(live, k) for k in (
            "PLUGIN_FORK_TASK", "DEFAULT_MAX_ITERATIONS", "MAX_ITERATIONS",
            "MAX_PROMPT_CHARS", "MAX_TIMEOUT_SECONDS")}
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
    print("ORACLE w11_pluginsess_a37456: 0 cases (link failure in disguise)")
    sys.exit(1)
print(f"ORACLE w11_pluginsess_a37456: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
