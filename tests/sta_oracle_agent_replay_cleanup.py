"""sta_oracle_agent_replay_cleanup.py — oracle for
agent/replay_cleanup.is_interrupted_tool_result. Replays each harness input
through the live Python function and compares the boolean exactly."""
import sys, json, os
sys.path.insert(0, os.path.expanduser("~/hermes-agent-dev"))
from agent.replay_cleanup import is_interrupted_tool_result

mism = 0
n = 0
for line in sys.stdin:
    line = line.strip()
    if not line.startswith("{"):
        continue
    rec = json.loads(line)
    inp = rec["in"]
    # Python signature takes the raw value (str or None).
    got = bool(rec["out"])
    exp = bool(is_interrupted_tool_result(inp))
    if got != exp:
        mism += 1
        print(f"MISMATCH in={inp!r} PY={exp!r} C={got!r}")
    n += 1
print(f"REPLAY_CLEANUP oracle: {n} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
