"""sta_oracle_agent_thinking_timeout_guidance.py — oracle for
agent/thinking_timeout_guidance.is_thinking_timeout. Replays each harness
input through the live Python function and compares the boolean exactly."""
import sys, json, os
sys.path.insert(0, os.path.expanduser("~/hermes-agent-dev"))
from agent.thinking_timeout_guidance import is_thinking_timeout

class Classified:
    def __init__(self, reason):
        self.reason = type("R", (), {"value": reason})()

mism = 0
n = 0
for line in sys.stdin:
    line = line.strip()
    if not line.startswith("{"):
        continue
    rec = json.loads(line)
    got = bool(rec["out"])
    exp = bool(is_thinking_timeout(
        Classified(rec["reason"]), rec["model"], rec["err"]))
    if got != exp:
        mism += 1
        print(f"MISMATCH rec={rec} PY={exp!r} C={got!r}")
    n += 1
print(f"THINKING_TIMEOUT_GUIDANCE oracle: {n} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
