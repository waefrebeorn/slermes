"""sta_oracle_agent_retry_utils.py — oracle for
agent/retry_utils.is_zai_coding_overload_error. Replays each harness input
through the live Python function and compares the boolean exactly."""
import sys, json, os
sys.path.insert(0, os.path.expanduser("~/hermes-agent-dev"))
from agent.retry_utils import is_zai_coding_overload_error

class Err:
    def __init__(self, status, text):
        self.status_code = status
        self._t = text
    # Python reads getattr(error, "status_code", None) and _error_text(error)
    # which joins message/body/response; our harness passes the lowercased
    # text directly which is what _error_text ultimately lowercases.
    def __getattr__(self, name):
        if name == "message":
            return self._t
        if name == "body":
            return self._t
        if name == "response":
            return self._t
        raise AttributeError(name)

mism = 0
n = 0
for line in sys.stdin:
    line = line.strip()
    if not line.startswith("{"):
        continue
    rec = json.loads(line)
    err = Err(rec["status"], rec["text"])
    got = bool(rec["out"])
    exp = bool(is_zai_coding_overload_error(
        base_url=rec["base"], model=rec["model"], error=err))
    if got != exp:
        mism += 1
        print(f"MISMATCH rec={rec} PY={exp!r} C={got!r}")
    n += 1
print(f"RETRY_UTILS oracle: {n} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
