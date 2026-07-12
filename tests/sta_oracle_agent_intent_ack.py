"""sta_oracle_agent_intent_ack.py — oracle for
agent/agent_runtime_helpers.intent_ack_continuation_mode. The Python helper
takes an agent object; we emulate it with a tiny object carrying the three
attributes the helper reads. Replays each harness input and compares the
resolved mode string exactly."""
import sys, json, os
sys.path.insert(0, os.path.expanduser("~/hermes-agent-dev"))
from agent.agent_runtime_helpers import intent_ack_continuation_mode

class Agent:
    def __init__(self, mode, api_mode, model):
        self._intent_ack_continuation = mode
        self.api_mode = api_mode
        self.model = model

mism = 0
n = 0
for line in sys.stdin:
    line = line.strip()
    if not line.startswith("{"):
        continue
    rec = json.loads(line)
    got = rec["out"]
    # Python accepts a list for codex;claude style -> pass actual list
    mode = rec["mode"]
    if mode and ";" in mode:
        mode = mode.split(";")
    exp = intent_ack_continuation_mode(Agent(mode, rec["api_mode"], rec["model"]))
    if got != exp:
        mism += 1
        print(f"MISMATCH rec={rec} PY={exp!r} C={got!r}")
    n += 1
print(f"INTENT_ACK oracle: {n} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
