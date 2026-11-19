"""twin for agent/rate_limit_tracker.py:has_data (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/rate_limit_tracker.py"
    PY_FN = "has_data"
    C_FN = "agent_rate_limit_tracker_has_data"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
