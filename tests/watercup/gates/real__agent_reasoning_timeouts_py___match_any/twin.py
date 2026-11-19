"""twin for agent/reasoning_timeouts.py:_match_any (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/reasoning_timeouts.py"
    PY_FN = "_match_any"
    C_FN = "floor"

    def norm(self, c, result):
        c.valstr("%.17g" % float(result))


_t = _Twin()
trace = _t.trace
run = _t.run
