"""twin for agent/reasoning_timeouts.py:get_reasoning_stale_timeout_floor (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/reasoning_timeouts.py"
    PY_FN = "get_reasoning_stale_timeout_floor"
    C_FN = "agent_reasoning_timeouts_get_floor"

    def norm(self, c, result):
        c.valstr("%.17g" % float(result))


_t = _Twin()
trace = _t.trace
run = _t.run
