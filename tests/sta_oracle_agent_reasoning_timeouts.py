"""sta_oracle_agent_reasoning_timeouts.py — oracle for
agent/reasoning_timeouts.get_reasoning_stale_timeout_floor.

Replays each harness input through the LIVE Python module and compares the
resulting floor exactly. Python returns Optional[float]; we serialize None
as JSON null and a float as its numeric value, matching the C harness output.
"""
import sys, json, os
sys.path.insert(0, os.path.expanduser("~/hermes-agent-dev"))
from agent.reasoning_timeouts import get_reasoning_stale_timeout_floor

mism = 0
n = 0
for line in sys.stdin:
    line = line.strip()
    if not line.startswith("{"):
        continue
    rec = json.loads(line)
    inp = rec["in"]
    got = rec["out"]
    exp = get_reasoning_stale_timeout_floor(inp)
    exp_json = (None if exp is None else exp)
    # Normalize: C emits floats; Python returns float or None. Compare via
    # JSON round-trip equality (null vs null, 600.0 vs 600.0).
    if got != exp_json and not (got is None and exp_json is None):
        if got is None or exp_json is None:
            mism += 1
            print(f"MISMATCH in={inp!r} PY={exp_json!r} C={got!r}")
        else:
            try:
                if float(got) != float(exp_json):
                    mism += 1
                    print(f"MISMATCH in={inp!r} PY={exp_json!r} C={got!r}")
            except (TypeError, ValueError):
                mism += 1
                print(f"MISMATCH in={inp!r} PY={exp_json!r} C={got!r}")
    n += 1
print(f"REASONING_TIMEOUTS oracle: {n} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
